import { performance } from "node:perf_hooks";
import { writeFile } from "node:fs/promises";
import { join } from "node:path";
import { setTimeout as delay } from "node:timers/promises";
import type { Brain } from "./brain.js";
import { direction, gameCommand, gift, message, parseDecision, resource, type Decision, type Step } from "./commands.js";
import { applyEvent, applyReply, initialState, MAX_LEVEL, note, renderState, ritualReady, snapshot, wantedHere, RESOURCE_CODES, type State } from "./game.js";
import { Journal } from "./history.js";
import { Meter } from "./metrics.js";
import { GameConnection, Message, type GameEvent, type GameReply } from "./protocol.js";

export { initialState } from "./game.js";

const DECISION_TIMEOUT = 120_000;
const ACTION_TIMEOUT = 10_000;
const MAX_INVALID = 3;
const MAX_AUTO_TAKES = 8;

export function ritualTimeout(level: number): number {
  return 30_000 * (level + 1) + 15_000;
}

function normalize(step: Step): Step {
  if (step.action === "MOVE" || step.action === "LOOK") return { action: step.action, arg: direction(step.arg).charAt(0) };
  if (step.action === "TAKE") return { action: step.action, arg: RESOURCE_CODES[resource(step.arg)]! };
  if (step.action === "ATTACK") return { action: step.action, arg: step.arg.trim().replace(/^#/, "") };
  if (step.action === "GIVE") {
    const { targetId, type } = gift(step.arg);
    return { action: step.action, arg: `${targetId}:${RESOURCE_CODES[type]!}` };
  }
  if (step.action === "BROADCAST") return { action: step.action, arg: message(step.arg) };
  return { action: step.action, arg: "" };
}

export type AgentOptions = {
  connection: GameConnection;
  brain: Brain;
  journal: Journal;
  runDirectory: string;
  maxDecisions: number;
  signal: AbortSignal;
  log?: (line: string) => void;
  verbose?: boolean;
};

export async function runAgent(options: AgentOptions): Promise<State> {
  const { connection, brain, journal, runDirectory, maxDecisions, signal } = options;
  const log = options.log ?? console.log;
  const verbose = options.verbose ?? false;
  const meter = new Meter(brain.pricing);
  const events: GameEvent[] = [];
  connection.onEvent = (event) => { events.push(event); };

  const hello = await connection.request(Message.HELLO, { name: brain.name }, ACTION_TIMEOUT, signal);
  const state = initialState(hello);
  await journal.append({ command: "HELLO", parameters: { name: brain.name }, reason: "Join the game", response: hello, durationMs: 0 });
  log(`Agent ${brain.name} #${state.agentId} (${brain.model}) connecté en (${state.x}, ${state.y}).`);

  async function save(status: string): Promise<void> {
    await writeFile(join(runDirectory, "session.json"), JSON.stringify({ status, provider: brain.name, model: brain.model, state: snapshot(state), metrics: meter.summary() }, null, 2), { encoding: "utf8", mode: 0o600 });
  }

  function drain(): { interrupt: string | null; died: boolean; ritualDone: boolean } {
    let interrupt: string | null = null;
    let died = false;
    let ritualDone = false;
    while (events.length > 0) {
      const event = events.shift()!;
      if (verbose) log(`  événement ${JSON.stringify(event)}`);
      const effect = applyEvent(state, event);
      interrupt ??= effect.interrupt;
      died ||= effect.died;
      ritualDone ||= effect.ritualDone;
    }
    return { interrupt, died, ritualDone };
  }

  async function act(step: Step, reason: string): Promise<{ reply: GameReply; interrupt: string | null }> {
    const request = gameCommand(step, state.agentId);
    const start = performance.now();
    let reply: GameReply;
    try {
      reply = await connection.request(request.type, request.payload, ACTION_TIMEOUT, signal);
    } catch (error) {
      await journal.append({ command: step.action, parameters: request.payload, reason,
        response: { ok: false, data: { outcome: "unknown", message: "Transport interrupted; command is not retried" } }, durationMs: performance.now() - start });
      throw error;
    }
    meter.countAction();
    await journal.append({ command: step.action, parameters: request.payload, reason, response: reply, durationMs: performance.now() - start });
    const outcome = applyReply(state, step.action, step.arg, reply);
    log(`  ${step.action}${step.arg ? ` ${step.arg}` : ""} ${reply.ok ? "OK" : `KO — ${String(reply.data)}`}${verbose && reply.ok ? ` ${JSON.stringify(reply.data)}` : ""}`);
    return { reply, interrupt: outcome.interrupt };
  }

  async function autoTake(): Promise<void> {
    for (let i = 0; i < MAX_AUTO_TAKES; ++i) {
      const wanted = wantedHere(state);
      if (wanted.length === 0) return;
      const { reply } = await act({ action: "TAKE", arg: RESOURCE_CODES[wanted[0]!]! }, "auto: ressource utile");
      if (!reply.ok) return;
    }
  }

  async function ritual(reason: string): Promise<string> {
    const { reply } = await act({ action: "RITUAL", arg: "" }, reason);
    if (!reply.ok) return "rituel refusé";
    const deadline = Date.now() + ritualTimeout(state.level);
    log(`  Rituel en cours vers le niveau ${state.level + 1}...`);
    while (Date.now() < deadline) {
      const effect = drain();
      if (effect.died) return "mort";
      if (effect.ritualDone) return `rituel terminé, niveau ${state.level}`;
      await delay(250, undefined, { signal });
    }
    note(state, "attente du rituel expirée");
    return "rituel sans confirmation";
  }

  async function maintain(): Promise<string | null> {
    await autoTake();
    if (state.level < MAX_LEVEL && ritualReady(state)) return ritual("auto: inventaire suffisant");
    return null;
  }

  async function execute(decision: Decision): Promise<{ interrupt: string | null; stop: boolean }> {
    for (const raw of decision.plan) {
      const step = normalize(raw);
      if (step.action === "STOP") return { interrupt: null, stop: true };
      if (step.action === "RITUAL") return { interrupt: await ritual(decision.why), stop: !state.alive };
      const { reply, interrupt } = await act(step, decision.why);
      let reason = interrupt;
      if (reply.ok && step.action === "MOVE") {
        const look = await act({ action: "LOOK", arg: step.arg }, "auto: observation après déplacement");
        reason ??= look.interrupt;
        const maintained = await maintain();
        reason ??= maintained;
      }
      const effect = drain();
      if (effect.died) return { interrupt: "mort", stop: true };
      reason ??= effect.interrupt;
      if (reason) return { interrupt: reason, stop: false };
    }
    return { interrupt: "plan terminé", stop: false };
  }

  await save("running");
  let interruption: string | null = "début de partie";
  let invalid = 0;
  try {
    const initial = await act({ action: "LOOK", arg: "N" }, "auto: observation initiale");
    interruption = initial.interrupt ?? interruption;
    interruption = (await maintain()) ?? interruption;
    for (let call = 0; call < maxDecisions; ++call) {
      signal.throwIfAborted();
      const effect = drain();
      if (effect.died || !state.alive) {
        log("Agent éliminé : arrêt.");
        await save("died");
        return state;
      }
      interruption = effect.interrupt ?? interruption;
      const prompt = renderState(state, maxDecisions - call, interruption);
      if (verbose) log(`--- prompt (${prompt.length} car.) ---\n${prompt}\n---`);
      const start = performance.now();
      const reply = await brain.decide(prompt, AbortSignal.any([signal, AbortSignal.timeout(DECISION_TIMEOUT)]));
      const latency = performance.now() - start;
      const usageLine = meter.record(reply.usage, latency, prompt.length);
      log(`  [IA] ${usageLine}`);
      let decision: Decision;
      try {
        decision = parseDecision(reply.text);
      } catch (error) {
        const message = error instanceof Error ? error.message : "Invalid decision";
        await journal.append({ command: "INVALID_DECISION", parameters: { text: reply.text.slice(0, 500) }, reason: "Decision rejected by client", response: { ok: false, data: message }, durationMs: latency });
        if (++invalid >= MAX_INVALID) throw new Error(`${brain.name} produced ${MAX_INVALID} invalid decisions; stopping`);
        interruption = `décision invalide: ${message}`;
        continue;
      }
      invalid = 0;
      state.memory = decision.memory;
      await journal.append({ command: "DECISION", parameters: { plan: decision.plan, memory: decision.memory }, reason: decision.why,
        response: { ok: true, data: { usage: reply.usage, latencyMs: Math.round(latency), promptChars: prompt.length } }, durationMs: latency });
      log(`[${call + 1}/${maxDecisions}] ${decision.plan.map((step) => `${step.action}${step.arg ? ` ${step.arg}` : ""}`).join(", ")} — ${decision.why}`);
      if (verbose) log(`  mémoire: ${decision.memory}`);
      const result = await execute(decision);
      if (result.stop) {
        log(state.alive ? "Arrêt demandé par l'IA." : "Agent éliminé : arrêt.");
        log(`Bilan : ${meter.describe()}`);
        await save(state.alive ? "stopped" : "died");
        return state;
      }
      interruption = result.interrupt;
      await save("running");
    }
    log("Limite de décisions atteinte.");
    log(`Bilan : ${meter.describe()}`);
    await save("decision_limit");
    return state;
  } catch (error) {
    log(`Bilan : ${meter.describe()}`);
    await save(signal.aborted ? "interrupted" : "failed");
    throw error;
  } finally {
    connection.onEvent = undefined;
  }
}
