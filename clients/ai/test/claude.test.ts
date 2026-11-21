import assert from "node:assert/strict";
import { test } from "node:test";
import type Anthropic from "@anthropic-ai/sdk";
import { claudeRequest, createClaudeBrain, readDecision, readUsage, type ClaudeRequest } from "../src/claude-brain.js";
import { GAME_INSTRUCTIONS, parseDecision } from "../src/commands.js";
import { cost, Meter } from "../src/metrics.js";
import { resolveModel } from "../src/models.js";

const decision = { plan: [{ action: "MOVE", arg: "N" }], memory: "Nord riche", why: "Explorer" };

function message(overrides: Partial<Anthropic.Beta.Messages.BetaMessage> = {}): Anthropic.Beta.Messages.BetaMessage {
  return {
    id: "msg_test",
    type: "message",
    role: "assistant",
    model: "claude-haiku-5-5",
    stop_reason: "end_turn",
    stop_details: null,
    content: [{ type: "text", text: JSON.stringify(decision) }],
    usage: { input_tokens: 300, output_tokens: 80, cache_read_input_tokens: 900, cache_creation_input_tokens: 0, output_tokens_details: { thinking_tokens: 40 } },
    ...overrides,
  } as Anthropic.Beta.Messages.BetaMessage;
}

test("each request is stateless with a cached system prompt and structured output", () => {
  const request = claudeRequest(resolveModel("haiku"), "low", "pos=1,1");
  assert.equal(request.model, "claude-haiku-5-5");
  assert.deepEqual(request.messages, [{ role: "user", content: "pos=1,1" }]);
  const system = request.system as Anthropic.Beta.Messages.BetaTextBlockParam[];
  assert.equal(system[0]!.text, GAME_INSTRUCTIONS);
  assert.deepEqual(system[0]!.cache_control, { type: "ephemeral" });
  assert.equal(request.output_config?.effort, "low");
  assert.equal(request.output_config?.format?.type, "json_schema");
  assert.equal(request.fallbacks, undefined);
  assert.equal(request.betas, undefined);
});

test("Opus, Sonnet and Fable opt into default refusal fallbacks", () => {
  for (const alias of ["opus", "sonnet", "fable"]) {
    const request = claudeRequest(resolveModel(alias), "medium", "x");
    assert.equal(request.fallbacks, "default");
    assert.deepEqual(request.betas, ["server-side-fallback-2026-07-01"]);
  }
});

test("Claude brain returns the decision and token usage", async () => {
  const requests: ClaudeRequest[] = [];
  const brain = createClaudeBrain("test-key", resolveModel("haiku"), "low", async (request) => {
    requests.push(request);
    return message();
  });
  const reply = await brain.decide("contexte", new AbortController().signal);
  assert.deepEqual(parseDecision(reply.text).plan, [{ action: "MOVE", arg: "N" }]);
  assert.deepEqual(reply.usage, { input: 300, output: 80, cacheRead: 900, cacheWrite: 0, thinking: 40 });
  await brain.decide("contexte 2", new AbortController().signal);
  assert.equal(requests.length, 2);
  assert.equal(requests[1]!.messages.length, 1);
  assert.equal(brain.model, "claude-haiku-5-5");
});

test("refusals, truncation and empty answers are rejected", () => {
  assert.throws(() => readDecision(message({ stop_reason: "refusal", stop_details: { type: "refusal", category: "cyber", explanation: null } as Anthropic.Beta.Messages.BetaMessage["stop_details"] })), /refusé.*cyber/);
  assert.throws(() => readDecision(message({ stop_reason: "max_tokens" })), /max_tokens/);
  assert.throws(() => readDecision(message({ content: [] })), /aucune décision/);
  assert.equal(readUsage(message({ usage: { input_tokens: 1, output_tokens: 2, cache_read_input_tokens: null, cache_creation_input_tokens: null, output_tokens_details: null } as unknown as Anthropic.Beta.Messages.BetaUsage }).usage).cacheRead, 0);
});

test("an already cancelled decision never calls the API", async () => {
  const brain = createClaudeBrain("test-key", resolveModel("opus"), "low", async () => { throw new Error("Should not start"); });
  await assert.rejects(brain.decide("context", AbortSignal.abort(new Error("already stopped"))), /already stopped/);
});

test("costs use per-model prices including cache reads and writes", () => {
  const opus = resolveModel("opus").pricing;
  assert.equal(cost({ input: 1_000_000, output: 0, cacheRead: 0, cacheWrite: 0, thinking: 0 }, opus), 4);
  assert.equal(cost({ input: 0, output: 1_000_000, cacheRead: 1_000_000, cacheWrite: 0, thinking: 0 }, opus), 20.2);
  assert.equal(cost({ input: 1, output: 1, cacheRead: 1, cacheWrite: 1, thinking: 0 }, null), null);
  const meter = new Meter(resolveModel("haiku").pricing);
  meter.record({ input: 1000, output: 100, cacheRead: 2000, cacheWrite: 0, thinking: 0 }, 500, 800);
  meter.countAction();
  meter.countAction();
  const summary = meter.summary();
  assert.equal(summary.calls, 1);
  assert.equal(summary.actions, 2);
  assert.ok(Math.abs(summary.costUsd! - 0.00017) < 1e-9);
  assert.match(meter.describe(), /2 actions/);
});
