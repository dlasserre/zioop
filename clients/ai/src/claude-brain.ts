import Anthropic from "@anthropic-ai/sdk";
import type { Brain, BrainReply } from "./brain.js";
import { decisionSchema, GAME_INSTRUCTIONS } from "./commands.js";
import type { Usage } from "./metrics.js";
import type { Effort, ModelInfo } from "./models.js";

const MAX_TOKENS = 8000;
const FALLBACK_BETA = "server-side-fallback-2026-07-01";

export type ClaudeRequest = Anthropic.Beta.Messages.MessageCreateParamsNonStreaming;
export type ClaudeSend = (request: ClaudeRequest, signal: AbortSignal) => Promise<Anthropic.Beta.Messages.BetaMessage>;

export function claudeRequest(model: ModelInfo, effort: Effort, prompt: string): ClaudeRequest {
  return {
    model: model.id,
    max_tokens: MAX_TOKENS,
    system: [{ type: "text", text: GAME_INSTRUCTIONS, cache_control: { type: "ephemeral" } }],
    messages: [{ role: "user", content: prompt }],
    output_config: { effort, format: { type: "json_schema", schema: decisionSchema as unknown as Record<string, unknown> } },
    ...(model.fallbacks ? { betas: [FALLBACK_BETA], fallbacks: "default" as const } : {}),
  };
}

export function readUsage(usage: Anthropic.Beta.Messages.BetaUsage): Usage {
  return {
    input: usage.input_tokens,
    output: usage.output_tokens,
    cacheRead: usage.cache_read_input_tokens ?? 0,
    cacheWrite: usage.cache_creation_input_tokens ?? 0,
    thinking: usage.output_tokens_details?.thinking_tokens ?? 0,
  };
}

export function readDecision(message: Anthropic.Beta.Messages.BetaMessage): string {
  if (message.stop_reason === "refusal") throw new Error(`Claude a refusé la requête (${message.stop_details?.category ?? "sans catégorie"})`);
  if (message.stop_reason === "max_tokens") throw new Error("Claude a atteint max_tokens avant de terminer sa décision");
  const text = message.content.filter((block) => block.type === "text").map((block) => block.text).join("");
  if (!text) throw new Error("Claude n'a renvoyé aucune décision");
  return text;
}

export function createClaudeBrain(apiKey: string, model: ModelInfo, effort: Effort, send?: ClaudeSend): Brain {
  const client = send ? null : new Anthropic({ apiKey, maxRetries: 2 });
  const transport: ClaudeSend = send ?? ((request, signal) => client!.beta.messages.create(request, { signal }));
  return {
    name: "Claude",
    model: model.id,
    pricing: model.pricing,
    async decide(prompt, signal): Promise<BrainReply> {
      signal.throwIfAborted();
      const message = await transport(claudeRequest(model, effort, prompt), signal);
      return { text: readDecision(message), usage: readUsage(message.usage) };
    },
  };
}
