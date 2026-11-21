export const EFFORTS = ["low", "medium", "high", "xhigh", "max"] as const;
export type Effort = typeof EFFORTS[number];

export type Pricing = { input: number; output: number; cacheRead: number; cacheWrite: number };

export type ModelInfo = {
  id: string;
  alias: string | null;
  pricing: Pricing | null;
  fallbacks: boolean;
};

const ALIASES: Record<string, { id: string; pricing: Pricing; fallbacks: boolean }> = {
  haiku: { id: "claude-haiku-5-5", pricing: { input: 0.1, output: 0.5, cacheRead: 0.01, cacheWrite: 0.125 }, fallbacks: false },
  sonnet: { id: "claude-sonnet-5-5", pricing: { input: 2, output: 10, cacheRead: 0.2, cacheWrite: 2.5 }, fallbacks: true },
  opus: { id: "claude-opus-5-5", pricing: { input: 4, output: 20, cacheRead: 0.2, cacheWrite: 5 }, fallbacks: true },
  fable: { id: "claude-fable-5-1", pricing: { input: 10, output: 50, cacheRead: 0.25, cacheWrite: 12.5 }, fallbacks: true },
};

export const MODEL_ALIASES = Object.keys(ALIASES);
export const DEFAULT_CLAUDE_MODEL = "opus";

export function resolveModel(name: string | undefined): ModelInfo {
  const wanted = (name ?? DEFAULT_CLAUDE_MODEL).trim();
  const alias = ALIASES[wanted.toLowerCase()];
  if (alias) return { id: alias.id, alias: wanted.toLowerCase(), pricing: alias.pricing, fallbacks: alias.fallbacks };
  const known = Object.entries(ALIASES).find(([, info]) => info.id === wanted);
  if (known) return { id: wanted, alias: known[0], pricing: known[1].pricing, fallbacks: known[1].fallbacks };
  if (!/^claude-[a-z0-9.-]+$/.test(wanted)) throw new Error(`model must be ${MODEL_ALIASES.join("|")} or a claude-* model id`);
  return { id: wanted, alias: null, pricing: null, fallbacks: false };
}

export function parseEffort(value: string | undefined): Effort {
  const effort = (value ?? "low").trim().toLowerCase();
  if (!(EFFORTS as readonly string[]).includes(effort)) throw new Error(`effort must be ${EFFORTS.join("|")}`);
  return effort as Effort;
}
