import type { Pricing } from "./models.js";

export type Usage = {
  input: number;
  output: number;
  cacheRead: number;
  cacheWrite: number;
  thinking: number;
};

export function emptyUsage(): Usage {
  return { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, thinking: 0 };
}

export function cost(usage: Usage, pricing: Pricing | null): number | null {
  if (!pricing) return null;
  return (usage.input * pricing.input + usage.output * pricing.output + usage.cacheRead * pricing.cacheRead + usage.cacheWrite * pricing.cacheWrite) / 1_000_000;
}

function dollars(value: number | null): string {
  return value === null ? "n/a" : `$${value.toFixed(5)}`;
}

export type MeterSummary = {
  calls: number;
  actions: number;
  usage: Usage;
  costUsd: number | null;
  costPerCallUsd: number | null;
  costPerActionUsd: number | null;
  averageLatencyMs: number;
  promptChars: number;
};

export class Meter {
  private readonly total = emptyUsage();
  private calls = 0;
  private actions = 0;
  private latency = 0;
  private promptChars = 0;

  constructor(private readonly pricing: Pricing | null) {}

  record(usage: Usage | null, latencyMs: number, promptChars: number): string {
    ++this.calls;
    this.latency += latencyMs;
    this.promptChars += promptChars;
    if (!usage) return `appel ${this.calls}: ${Math.round(latencyMs)} ms, usage indisponible`;
    for (const key of Object.keys(this.total) as (keyof Usage)[]) this.total[key] += usage[key];
    const callCost = cost(usage, this.pricing);
    return `appel ${this.calls}: in=${usage.input} cache_lu=${usage.cacheRead} cache_écrit=${usage.cacheWrite} out=${usage.output} (réflexion ${usage.thinking}) prompt=${promptChars} car. ${Math.round(latencyMs)} ms coût=${dollars(callCost)} total=${dollars(cost(this.total, this.pricing))}`;
  }

  countAction(): void {
    ++this.actions;
  }

  summary(): MeterSummary {
    const total = cost(this.total, this.pricing);
    return {
      calls: this.calls,
      actions: this.actions,
      usage: { ...this.total },
      costUsd: total,
      costPerCallUsd: total === null || this.calls === 0 ? null : total / this.calls,
      costPerActionUsd: total === null || this.actions === 0 ? null : total / this.actions,
      averageLatencyMs: this.calls === 0 ? 0 : Math.round(this.latency / this.calls),
      promptChars: this.promptChars,
    };
  }

  describe(): string {
    const s = this.summary();
    const read = s.usage.cacheRead + s.usage.cacheWrite + s.usage.input;
    const hit = read === 0 ? 0 : Math.round((100 * s.usage.cacheRead) / read);
    return `${s.calls} appels IA pour ${s.actions} actions de jeu (${s.calls === 0 ? 0 : (s.actions / s.calls).toFixed(1)} actions/appel), tokens in=${s.usage.input} cache_lu=${s.usage.cacheRead} (${hit}%) cache_écrit=${s.usage.cacheWrite} out=${s.usage.output}, coût=${dollars(s.costUsd)} soit ${dollars(s.costPerCallUsd)}/appel et ${dollars(s.costPerActionUsd)}/action, latence moyenne ${s.averageLatencyMs} ms`;
  }
}
