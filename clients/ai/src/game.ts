import type { GameEvent, GameReply } from "./protocol.js";

export const RESOURCE_CODES = ["a", "c", "m", "w"];
export const RESOURCE_NAMES = ["pomme", "gateau", "viande", "eau"];
export const REQUIREMENTS: number[][] = [
  [10, 5, 2, 1],
  [20, 10, 4, 2],
  [40, 15, 8, 3],
];
export const MAX_LEVEL = REQUIREMENTS.length + 1;
export const INVOCATION_COSTS: number[][] = [
  [20, 5, 1, 0],
  [30, 6, 2, 0],
  [40, 7, 3, 0],
  [50, 8, 4, 0],
];
const BASE_HP = 100;
const HP_PER_LEVEL = 25;

export function maxHealth(level: number): number {
  return BASE_HP + (Math.max(1, level) - 1) * HP_PER_LEVEL;
}
const MAP_RADIUS = 5;
const RECENT_LINES = 8;
const RECENT_MESSAGES = 3;
const MAX_KNOWN_RESOURCES = 12;
const MAX_KNOWN_AGENTS = 8;

export type KnownAgent = { level: number; health: number; x: number; y: number; seenAt: number };

export type State = {
  agentId: number;
  level: number;
  health: number;
  x: number;
  y: number;
  inventory: number[];
  alive: boolean;
  visited: Set<string>;
  resources: Map<string, number[]>;
  agents: Map<number, KnownAgent>;
  allies: Set<number>;
  maxX: number | null;
  maxY: number | null;
  kills: number;
  memory: string;
  actions: number;
  recent: string[];
  messages: string[];
  ritualLevel: number | null;
};

function integer(value: unknown, minimum = 0): number {
  if (!Number.isSafeInteger(value) || Number(value) < minimum) throw new Error("Invalid integer from game server");
  return value as number;
}

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Expected a JSON object from game server");
  return value as Record<string, unknown>;
}

function key(x: number, y: number): string {
  return `${x},${y}`;
}

export function initialState(reply: GameReply): State {
  if (!reply.ok) throw new Error(`HELLO rejected: ${String(reply.data)}`);
  const data = record(reply.data);
  const position = record(data.position);
  const state: State = {
    agentId: integer(data.agentId, 1),
    level: 1,
    health: 100,
    x: integer(position.x),
    y: integer(position.y),
    inventory: [0, 0, 0, 0],
    alive: true,
    visited: new Set(),
    resources: new Map(),
    agents: new Map(),
    allies: new Set(),
    maxX: null,
    maxY: null,
    kills: 0,
    memory: "",
    actions: 0,
    recent: [],
    messages: [],
    ritualLevel: null,
  };
  state.visited.add(key(state.x, state.y));
  return state;
}

export function missing(state: State): number[] | null {
  const cost = REQUIREMENTS[state.level - 1];
  if (!cost) return null;
  return cost.map((needed, type) => Math.max(0, needed - (state.inventory[type] ?? 0)));
}

export function ritualReady(state: State): boolean {
  const need = missing(state);
  return need !== null && need.every((count) => count === 0);
}

export function wantedHere(state: State): number[] {
  const need = missing(state);
  const here = state.resources.get(key(state.x, state.y)) ?? [];
  if (!need) return [];
  const wanted: number[] = [];
  const left = [...need];
  for (const type of here) {
    if ((left[type] ?? 0) > 0) {
      wanted.push(type);
      left[type] = (left[type] ?? 0) - 1;
    }
  }
  return wanted;
}

export function note(state: State, line: string): void {
  state.recent.push(line);
  if (state.recent.length > RECENT_LINES) state.recent.splice(0, state.recent.length - RECENT_LINES);
}

function readInventory(state: State, resources: unknown): void {
  if (!Array.isArray(resources)) throw new Error("Invalid inventory from server");
  const inventory = [0, 0, 0, 0];
  for (const entry of resources) {
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) continue;
    const item = entry as Record<string, unknown>;
    const type = Number(item.type);
    if (Number.isInteger(type) && type >= 0 && type < inventory.length) inventory[type] = integer(item.quantity);
  }
  state.inventory = inventory;
}

function readLook(state: State, cells: unknown): string[] {
  if (!Array.isArray(cells)) throw new Error("Invalid LOOK from server");
  const newcomers: string[] = [];
  for (const value of cells) {
    const cell = record(value);
    if (!Array.isArray(cell.position) || cell.position.length !== 2) throw new Error("Invalid cell position from game server");
    const x = integer(cell.position[0]);
    const y = integer(cell.position[1]);
    const types = Array.isArray(cell.resources) ? cell.resources.map((resource) => integer(record(resource).type)).filter((type) => type < RESOURCE_CODES.length) : [];
    if (types.length > 0) state.resources.set(key(x, y), types);
    else state.resources.delete(key(x, y));
    for (const [id, known] of state.agents) {
      if (known.x === x && known.y === y) state.agents.delete(id);
    }
    for (const agent of Array.isArray(cell.agents) ? cell.agents : []) {
      const seen = record(agent);
      const id = integer(seen.id, 1);
      if (id === state.agentId) continue;
      if (x === state.x && y === state.y) newcomers.push(`#${id}`);
      state.agents.set(id, { level: integer(seen.level, 1), health: Number.isSafeInteger(seen.health) ? Number(seen.health) : 100, x, y, seenAt: state.actions });
    }
  }
  return newcomers;
}

export type Outcome = { interrupt: string | null };

export function applyReply(state: State, action: string, arg: string, reply: GameReply): Outcome {
  ++state.actions;
  if (!reply.ok) {
    const reason = String(reply.data);
    if (action === "MOVE") {
      if (arg === "E" && state.maxX === null) state.maxX = state.x;
      if (arg === "S" && state.maxY === null) state.maxY = state.y;
    }
    if (action === "TAKE") state.resources.delete(key(state.x, state.y));
    note(state, `${action} ${arg} KO: ${reason}`.replace(/\s+/g, " ").trim());
    return { interrupt: action === "TAKE" ? null : `${action} refusé` };
  }
  const data = record(reply.data);
  if (action === "MOVE") {
    const position = record(data.position);
    state.x = integer(position.x);
    state.y = integer(position.y);
    state.visited.add(key(state.x, state.y));
    note(state, `MOVE ${arg} -> ${state.x},${state.y}`);
    return { interrupt: null };
  }
  if (action === "LOOK") {
    const newcomers = readLook(state, data.cells);
    const here = state.resources.get(key(state.x, state.y)) ?? [];
    note(state, `LOOK ${arg}: ${here.length > 0 ? `ici ${here.map((type) => RESOURCE_CODES[type]).join("")}` : "rien ici"}${newcomers.length > 0 ? `, agents ${newcomers.join(" ")}` : ""}`);
    return { interrupt: newcomers.length > 0 ? `agents sur ta case: ${newcomers.join(" ")}` : null };
  }
  if (action === "TAKE" || action === "INVENTORY") {
    readInventory(state, data.resources);
    if (action === "TAKE") {
      const here = state.resources.get(key(state.x, state.y));
      const type = RESOURCE_CODES.indexOf(arg);
      if (here) {
        const index = here.indexOf(type);
        if (index >= 0) here.splice(index, 1);
        if (here.length === 0) state.resources.delete(key(state.x, state.y));
      }
    }
    note(state, `${action} ${arg} ok`.trim());
    return { interrupt: null };
  }
  if (action === "RITUAL") {
    const cost = REQUIREMENTS[state.level - 1] ?? [0, 0, 0, 0];
    state.inventory = state.inventory.map((count, type) => Math.max(0, count - (cost[type] ?? 0)));
    state.ritualLevel = state.level + 1;
    note(state, `RITUAL lancé vers niveau ${state.ritualLevel}`);
    return { interrupt: null };
  }
  if (action === "ATTACK") {
    const targetId = integer(data.targetId, 1);
    if (data.killed === true) {
      state.agents.delete(targetId);
      ++state.kills;
    }
    note(state, `ATTACK #${targetId} ${data.killed === true ? "tué" : "touché"}`);
    return { interrupt: null };
  }
  if (action === "HEAL") {
    if (data.success !== true) {
      note(state, "HEAL KO: pas d'eau");
      return { interrupt: "HEAL refusé" };
    }
    state.inventory[3] = Math.max(0, (state.inventory[3] ?? 0) - 1);
    state.health = maxHealth(state.level);
    note(state, "HEAL ok");
    return { interrupt: null };
  }
  if (action === "INVOCATION") {
    const spawned = integer(data.spawnedAgentId, 1);
    const cost = INVOCATION_COSTS[state.level - 1] ?? [0, 0, 0, 0];
    state.inventory = state.inventory.map((count, type) => Math.max(0, count - (cost[type] ?? 0)));
    state.allies.add(spawned);
    const position = Array.isArray(data.position) ? data.position : [];
    note(state, `INVOCATION allié #${spawned}${position.length === 2 ? ` en ${String(position[0])},${String(position[1])}` : ""}`);
    return { interrupt: null };
  }
  if (action === "GIVE") {
    const type = integer(data.resource);
    if (type < state.inventory.length) state.inventory[type] = Math.max(0, (state.inventory[type] ?? 0) - 1);
    note(state, `GIVE ${arg} ok`);
    return { interrupt: null };
  }
  if (action === "BROADCAST") {
    note(state, `BROADCAST envoyé`);
    return { interrupt: null };
  }
  return { interrupt: null };
}

export type EventEffect = { interrupt: string | null; died: boolean; ritualDone: boolean };

export function applyEvent(state: State, event: GameEvent): EventEffect {
  const effect: EventEffect = { interrupt: null, died: false, ritualDone: false };
  if (event.agentId !== state.agentId) return effect;
  if (event.type === "Died") {
    state.alive = false;
    effect.died = true;
    note(state, "MORT");
  } else if (event.type === "Ritual") {
    if (Number.isSafeInteger(event.level)) state.level = Number(event.level);
    state.health = maxHealth(state.level);
    state.ritualLevel = null;
    effect.ritualDone = true;
    note(state, `RITUEL terminé: niveau ${state.level}`);
  } else if (event.type === "RottenResources" && Array.isArray(event.resources)) {
    const lost = [0, 0, 0, 0];
    for (const type of event.resources) {
      if (Number.isInteger(type) && type >= 0 && type < lost.length) {
        lost[type] = (lost[type] ?? 0) + 1;
        state.inventory[type] = Math.max(0, (state.inventory[type] ?? 0) - 1);
      }
    }
    note(state, `pourri: ${lost.map((count, type) => (count > 0 ? `${RESOURCE_CODES[type]}${count}` : "")).join("")}`);
  } else if (event.type === "Message" && typeof event.message === "string") {
    const line = `#${String(event.speakerId)}${typeof event.direction === "number" ? ` (dir ${event.direction})` : " (ta case)"}: ${event.message.slice(0, 120)}`;
    state.messages.push(line);
    if (state.messages.length > RECENT_MESSAGES) state.messages.splice(0, state.messages.length - RECENT_MESSAGES);
    effect.interrupt = "message reçu";
  }
  return effect;
}

function inventoryText(counts: number[]): string {
  return counts.map((count, type) => `${RESOURCE_CODES[type]}${count}`).join(" ");
}

function localMap(state: State): string {
  const rows: string[] = [];
  const occupied = new Set([...state.agents.values()].map((agent) => key(agent.x, agent.y)));
  for (let y = state.y - MAP_RADIUS; y <= state.y + MAP_RADIUS; ++y) {
    let row = "";
    for (let x = state.x - MAP_RADIUS; x <= state.x + MAP_RADIUS; ++x) {
      const outside = x < 0 || y < 0 || (state.maxX !== null && x > state.maxX) || (state.maxY !== null && y > state.maxY);
      if (outside) row += "#";
      else if (x === state.x && y === state.y) row += "@";
      else if (occupied.has(key(x, y))) row += "A";
      else if (state.resources.has(key(x, y))) row += "r";
      else if (state.visited.has(key(x, y))) row += ".";
      else row += "?";
    }
    rows.push(row);
  }
  return rows.join("\n");
}

function distance(state: State, x: number, y: number): number {
  return Math.abs(state.x - x) + Math.abs(state.y - y);
}

export function renderState(state: State, remainingCalls: number, interruption: string | null): string {
  const need = missing(state);
  const resources = [...state.resources.entries()]
    .map(([where, types]) => {
      const [x, y] = where.split(",").map(Number) as [number, number];
      return { where, types, d: distance(state, x, y) };
    })
    .sort((a, b) => a.d - b.d)
    .slice(0, MAX_KNOWN_RESOURCES)
    .map(({ where, types }) => `${where}:${types.map((type) => RESOURCE_CODES[type]).join("")}`);
  const agents = [...state.agents.entries()]
    .sort((a, b) => distance(state, a[1].x, a[1].y) - distance(state, b[1].x, b[1].y))
    .slice(0, MAX_KNOWN_AGENTS)
    .map(([id, agent]) => `#${id}${state.allies.has(id) ? " allié" : ""} niv${agent.level} pv${agent.health} en ${agent.x},${agent.y} (vu il y a ${state.actions - agent.seenAt} act.)`);
  const lines = [
    `pos=${state.x},${state.y} niv=${state.level} pv=${state.health} inv=${inventoryText(state.inventory)} manque=${need ? inventoryText(need) : "niveau max"} tués=${state.kills}`,
    `bords: x max ${state.maxX ?? "?"}, y max ${state.maxY ?? "?"} ; cases visitées ${state.visited.size} ; appels restants ${remainingCalls}`,
    `carte (rayon ${MAP_RADIUS}, ligne 1 = y ${state.y - MAP_RADIUS}):`,
    localMap(state),
    `ressources connues: ${resources.length > 0 ? resources.join(" ") : "aucune"}`,
    `agents connus: ${agents.length > 0 ? agents.join(" ; ") : "aucun"}`,
  ];
  if (state.messages.length > 0) lines.push(`messages: ${state.messages.join(" | ")}`);
  lines.push(`dernières actions: ${state.recent.length > 0 ? state.recent.join(" ; ") : "aucune"}`);
  if (interruption) lines.push(`rappel car: ${interruption}`);
  lines.push(`mémoire: ${state.memory || "(vide)"}`);
  return lines.join("\n");
}

export function snapshot(state: State): Record<string, unknown> {
  return {
    agentId: state.agentId,
    level: state.level,
    position: { x: state.x, y: state.y },
    inventory: state.inventory,
    alive: state.alive,
    kills: state.kills,
    visited: state.visited.size,
    memory: state.memory,
    actions: state.actions,
  };
}
