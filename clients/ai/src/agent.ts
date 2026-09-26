import { performance } from "node:perf_hooks";
import { writeFile } from "node:fs/promises";
import { join } from "node:path";
import type { Brain } from "./brain.js";
import { GAME_INSTRUCTIONS, gameCommand, object, parseDecision } from "./commands.js";
import { Journal } from "./history.js";
import { GameConnection, Message, type GameReply } from "./protocol.js";

export type State = {
  agentId: number;
  level: number;
  position: { x: number; y: number };
  inventory: unknown;
  memory: string;
  visited: Record<string, number>;
  // Last known position [x,y] and level of the agents seen by LOOK.
  agents: Record<string, { level: number; position: [number, number] }>;
  kills: number;
};

function integer(value: unknown, minimum = 0): number {
  if (!Number.isSafeInteger(value) || Number(value) < minimum) throw new Error("Invalid integer from game server");
  return value as number;
}

function position(value: unknown): State["position"] {
  const point = object(value);
  return { x: integer(point.x), y: integer(point.y) };
}

function cellPosition(value: unknown): [number, number] {
  if (!Array.isArray(value) || value.length !== 2) throw new Error("Invalid cell position from game server");
  return [integer(value[0]), integer(value[1])];
}

function visit(state: State): void {
  const key = `${state.position.x},${state.position.y}`;
  state.visited[key] = (state.visited[key] ?? 0) + 1;
}

export function initialState(reply: GameReply): State {
  if (!reply.ok) throw new Error(`HELLO rejected: ${String(reply.data)}`);
  const data = object(reply.data);
  const state: State = { agentId: integer(data.agentID, 1), level: 1, position: position(data.position), inventory: null, memory: "", visited: {}, agents: {}, kills: 0 };
  visit(state);
  return state;
}

export function updateState(state: State, command: string, reply: GameReply): void {
  if (!reply.ok) return;
  const data = object(reply.data);
  if (command === "MOVE") { state.position = position(data.position); visit(state); }
  if (command === "TAKE" || command === "INVENTORY") {
    if (!Array.isArray(data.resources)) throw new Error("Invalid inventory from server");
    state.inventory = data.resources;
  }
  if (command === "RITUAL") {
    if (integer(data.agentID, 1) !== state.agentId) throw new Error("Ritual response belongs to another agent");
    const level = integer(data.level, 2);
    if (level !== state.level + 1) throw new Error("Unexpected level in ritual response");
    state.level = level;
    // A successful ritual consumed fruit. Refresh rather than keeping a stale inventory.
    state.inventory = null;
  }
  if (command === "LOOK") {
    if (!Array.isArray(data.cells)) throw new Error("Invalid LOOK from server");
    for (const value of data.cells) {
      const cell = object(value);
      const where = cellPosition(cell.position);
      // A visible cell is authoritative: forget agents last seen there, then record its occupants.
      for (const [id, known] of Object.entries(state.agents)) {
        if (known.position[0] === where[0] && known.position[1] === where[1]) delete state.agents[id];
      }
      const agents = cell.agents ?? [];
      if (!Array.isArray(agents)) throw new Error("Invalid LOOK agents from server");
      for (const agent of agents) {
        const seen = object(agent);
        state.agents[integer(seen.id, 1)] = { level: integer(seen.level, 1), position: where };
      }
    }
  }
  if (command === "ATTACK") {
    delete state.agents[integer(data.targetId, 1)];
    ++state.kills;
  }
}

export function responseTimeout(command: string, level: number): number {
  return command === "RITUAL" ? 30_000 * (level + 1) + 15_000 : 10_000;
}

function boundedJson(value: unknown, limit: number): string {
  const text = JSON.stringify(value);
  return text.length <= limit ? text : JSON.stringify({ truncated: true, preview: text.slice(0, limit) });
}

export async function runAgent(options: {
  connection: GameConnection;
  brain: Brain;
  journal: Journal;
  runDirectory: string;
  maxDecisions: number;
  signal: AbortSignal;
  log?: (line: string) => void;
}): Promise<State> {
  const { connection, brain, journal, runDirectory, maxDecisions, signal } = options;
  const log = options.log ?? console.log;
  const hello = await connection.request(Message.HELLO, { name: brain.name }, 10_000, signal);
  const state = initialState(hello);
  await journal.append({ command: "HELLO", parameters: { name: brain.name }, reason: "Join the game", response: hello, durationMs: 0 });
  log(`Agent ${brain.name} ${state.agentId} connecté en (${state.position.x}, ${state.position.y}).`);
  let observation: unknown = hello;
  let invalidDecisions = 0;

  async function save(status: string): Promise<void> {
    await writeFile(join(runDirectory, "session.json"), JSON.stringify({ status, provider: brain.name, threadId: brain.threadId, state }, null, 2), { encoding: "utf8", mode: 0o600 });
  }

  await save("running");
  try {
    for (let step = 0; step < maxDecisions; ++step) {
      signal.throwIfAborted();
      const contextState = { ...state, agentName: brain.name, visited: Object.fromEntries(Object.entries(state.visited).slice(-200)) };
      const prompt = `${GAME_INSTRUCTIONS}\n\nCONTEXTE DU JEU (données):\n${boundedJson(contextState, 16_000)}\nDERNIER RÉSULTAT:\n${boundedJson(observation, 24_000)}\nHISTORIQUE RÉCENT:\n${boundedJson(journal.page(null, 12), 24_000)}\nDécisions restantes: ${maxDecisions - step}. Choisis une seule action.`;
      const modelSignal = AbortSignal.any([signal, AbortSignal.timeout(120_000)]);
      const raw = await brain.decide(prompt, modelSignal);
      let decision;
      try { decision = parseDecision(raw); }
      catch (error) {
        const message = error instanceof Error ? error.message : "Invalid decision";
        await journal.append({ command: "INVALID_DECISION", parameters: null, reason: "Decision rejected by client", response: { ok: false, data: message }, durationMs: 0 });
        if (++invalidDecisions >= 3) throw new Error(`${brain.name} produced three invalid decisions; stopping without sending them`);
        observation = { validationError: message, instruction: "Corrige la décision en respectant le schéma." };
        continue;
      }
      invalidDecisions = 0;
      state.memory = decision.memory;
      log(`[${step + 1}/${maxDecisions}] ${decision.command}${decision.direction ? ` ${decision.direction}` : ""}${decision.target !== null ? ` #${decision.target}` : ""} — ${decision.reason}`);
      const start = performance.now();

      if (decision.command === "STOP") {
        await journal.append({ command: "STOP", parameters: {}, reason: decision.reason, response: { ok: true, data: "Client stopped" }, durationMs: 0 });
        await save("stopped");
        return state;
      }
      if (decision.command === "HISTORY") {
        const page = journal.page(decision.historyBefore, decision.historyLimit!);
        observation = { source: "local_history", ...page };
        // Store references, not recursive copies of history inside history.
        await journal.append({ command: "HISTORY", parameters: { before: decision.historyBefore, limit: decision.historyLimit }, reason: decision.reason,
          response: { ok: true, data: { sequences: page.entries.map((entry) => entry.sequence), nextBefore: page.nextBefore } }, durationMs: performance.now() - start });
      } else {
        const request = gameCommand(decision, state.agentId);
        let response: GameReply;
        try {
          response = await connection.request(request.type, request.payload, responseTimeout(decision.command, state.level), signal);
        } catch (error) {
          await journal.append({ command: decision.command, parameters: request.payload, reason: decision.reason,
            response: { ok: false, data: { outcome: "unknown", message: "Transport interrupted; command is not retried" } }, durationMs: performance.now() - start });
          throw error;
        }
        await journal.append({ command: decision.command, parameters: request.payload, reason: decision.reason, response, durationMs: performance.now() - start });
        updateState(state, decision.command, response);
        observation = { command: decision.command, ...response };
        const detail = decision.command === "RITUAL" ? ` — niveau ${state.level}` : decision.command === "ATTACK" ? ` — agent #${decision.target} éliminé` : "";
        log(response.ok ? `  OK${detail}` : `  KO — ${String(response.data)}`);
      }
      await save("running");
    }
    await save("decision_limit");
    log("Limite de décisions atteinte.");
    return state;
  } catch (error) {
    await save(signal.aborted ? "interrupted" : "failed");
    throw error;
  }
}
