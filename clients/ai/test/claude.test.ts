import assert from "node:assert/strict";
import { test, type TestContext } from "node:test";
import { mkdtemp, rm } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import type { SDKMessage, SDKResultMessage } from "@anthropic-ai/claude-agent-sdk";
import { claudeOptions, createClaudeBrain, type ClaudeQueryFactory } from "../src/claude-brain.js";
import { parseDecision } from "../src/commands.js";

async function directory(t: TestContext): Promise<string> {
  const path = await mkdtemp(join(tmpdir(), "zioop-claude-test-"));
  t.after(() => rm(path, { recursive: true, force: true }));
  return path;
}

const action = { command: "INVENTORY", direction: null, target: null, historyBefore: null, historyLimit: null, reason: "Vérifier mes fruits", memory: "Position connue" };

// Only SDK event fields consumed by the adapter are needed in these fixtures.
function result(overrides: Partial<SDKResultMessage> = {}): SDKMessage {
  return { type: "result", subtype: "success", is_error: false, session_id: "claude-session", structured_output: action, ...overrides } as SDKMessage;
}

test("Claude uses its API key and disables inherited tools and settings", () => {
  const options = claudeOptions("test-anthropic-key", "/session", {
    PATH: "runtime", OPENAI_API_KEY: "other-key", ANTHROPIC_AUTH_TOKEN: "personal-login", CLAUDE_CONFIG_DIR: "/personal", GITHUB_TOKEN: "private",
  }, "chosen-model");
  assert.equal(options.model, "chosen-model");
  assert.equal(options.env?.ANTHROPIC_API_KEY, "test-anthropic-key");
  assert.equal(options.env?.PATH, "runtime");
  assert.equal(options.env?.CLAUDE_CONFIG_DIR, join("/session", "claude-home"));
  assert.equal(options.env?.ANTHROPIC_AUTH_TOKEN, undefined);
  assert.equal(options.env?.OPENAI_API_KEY, undefined);
  assert.equal(options.env?.GITHUB_TOKEN, undefined);
  assert.deepEqual(options.tools, []);
  assert.deepEqual(options.settingSources, []);
  assert.deepEqual(options.plugins, []);
  assert.deepEqual(options.skills, []);
  assert.deepEqual(options.mcpServers, {});
  assert.equal(options.strictMcpConfig, true);
  assert.equal(options.permissionMode, "dontAsk");
  assert.equal(options.persistSession, true);
  assert.equal(options.outputFormat?.type, "json_schema");
});

test("Claude resumes the same session and returns structured decisions", async (t) => {
  const calls: Parameters<ClaudeQueryFactory>[0][] = [];
  let closed = 0;
  const factory: ClaudeQueryFactory = (args) => {
    calls.push(args);
    return {
      async *[Symbol.asyncIterator]() { yield result(); },
      close() { ++closed; },
    };
  };
  const brain = await createClaudeBrain("test-key", await directory(t), undefined, factory);
  const signal = new AbortController().signal;
  assert.equal(brain.threadId, null);
  assert.deepEqual(parseDecision(await brain.decide("premier contexte", signal)), action);
  assert.equal(brain.threadId, "claude-session");
  await brain.decide("résultat réel + historique", signal);
  assert.equal(calls[0]!.options.resume, undefined);
  assert.equal(calls[1]!.options.resume, "claude-session");
  assert.equal(calls[1]!.prompt, "résultat réel + historique");
  assert.equal(closed, 2);
});

test("Claude rejects API errors, missing JSON and exhausted structured retries", async (t) => {
  const runDirectory = await directory(t);
  const cases: [SDKMessage[], RegExp][] = [
    [[result({ structured_output: undefined })], /no valid structured decision/],
    [[result({ is_error: true, result: "API key rejected" })], /API key rejected/],
    [[result({ subtype: "error_max_structured_output_retries", errors: ["Invalid JSON"] })], /Invalid JSON/],
    [[], /without a decision/],
  ];
  for (const [messages, expected] of cases) {
    let closed = false;
    const factory: ClaudeQueryFactory = () => ({
      async *[Symbol.asyncIterator]() { yield* messages; },
      close() { closed = true; },
    });
    const brain = await createClaudeBrain("test-key", runDirectory, undefined, factory);
    await assert.rejects(brain.decide("context", new AbortController().signal), expected);
    assert.equal(closed, true);
  }
});

test("Claude cancellation reaches the SDK and closes the running query", async (t) => {
  let closed = false;
  let started!: () => void;
  const ready = new Promise<void>((resolve) => { started = resolve; });
  const factory: ClaudeQueryFactory = ({ options }) => ({
    async *[Symbol.asyncIterator]() {
      const signal = options.abortController!.signal;
      await new Promise<never>((_resolve, reject) => {
        signal.addEventListener("abort", () => reject(signal.reason), { once: true });
        started();
      });
    },
    close() { closed = true; },
  });
  const brain = await createClaudeBrain("test-key", await directory(t), undefined, factory);
  const controller = new AbortController();
  const pending = brain.decide("context", controller.signal);
  const rejected = assert.rejects(pending, /requested stop/);
  await ready;
  controller.abort(new Error("requested stop"));
  await rejected;
  assert.equal(closed, true);
});

test("an already cancelled Claude decision never starts a query", async (t) => {
  const factory: ClaudeQueryFactory = () => { throw new Error("Should not start"); };
  const brain = await createClaudeBrain("test-key", await directory(t), undefined, factory);
  await assert.rejects(brain.decide("context", AbortSignal.abort(new Error("already stopped"))), /already stopped/);
});
