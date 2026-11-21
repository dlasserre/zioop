import assert from "node:assert/strict";
import { test, type TestContext } from "node:test";
import { createServer, type Socket } from "node:net";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { setTimeout as delay } from "node:timers/promises";
import { FrameDecoder, GameConnection, Message, encode } from "../src/protocol.js";
import { gameCommand, parseDecision, type Step } from "../src/commands.js";
import { Journal, readJournal } from "../src/history.js";
import { ritualTimeout, runAgent } from "../src/agent.js";
import { applyEvent, applyReply, initialState, missing, renderState, ritualReady, wantedHere } from "../src/game.js";
import { sdkOptions, threadOptions, type Brain } from "../src/brain.js";
import { readConfig, redact } from "../src/config.js";

function plan(steps: [Step["action"], string?][], memory = "Remember the fruit"): string {
  return JSON.stringify({ plan: steps.map(([action, arg]) => ({ action, arg: arg ?? "" })), memory, why: "Test" });
}

function brain(replies: string[], prompts: string[] = []): Brain {
  return {
    name: "Test",
    model: "test-model",
    pricing: { input: 1, output: 1, cacheRead: 1, cacheWrite: 1 },
    async decide(prompt) {
      prompts.push(prompt);
      return { text: replies.shift() ?? plan([["STOP"]]), usage: { input: 10, output: 5, cacheRead: 100, cacheWrite: 0, thinking: 2 } };
    },
  };
}

function rawFrame(type: number, text: string): Buffer {
  const body = Buffer.from(text, "utf8");
  const header = Buffer.alloc(6);
  header.writeUInt16BE(type, 0);
  header.writeUInt32BE(body.length, 2);
  return Buffer.concat([header, body]);
}

async function fixture(t: TestContext, handler: (type: number, data: Record<string, unknown>, socket: Socket) => void) {
  const sockets = new Set<Socket>();
  const server = createServer((socket) => {
    sockets.add(socket);
    socket.on("close", () => sockets.delete(socket));
    socket.on("error", () => {});
    const decoder = new FrameDecoder();
    socket.on("data", (chunk: Buffer) => {
      for (const frame of decoder.push(chunk)) handler(frame.type, JSON.parse(frame.payload) as Record<string, unknown>, socket);
    });
  });
  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  const address = server.address();
  assert(address && typeof address === "object");
  t.after(async () => {
    for (const socket of sockets) socket.destroy();
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });
  const connection = new GameConnection();
  t.after(() => connection.close());
  await connection.connect("127.0.0.1", address.port, new AbortController().signal);
  return connection;
}

async function directory(t: TestContext): Promise<string> {
  const path = await mkdtemp(join(tmpdir(), "zioop-client-test-"));
  t.after(() => rm(path, { recursive: true, force: true }));
  return path;
}

test("binary protocol supports fragmented and coalesced UTF-8 frames", () => {
  const first = encode(0, { message: "récolté" });
  assert.equal(first.readUInt32BE(2), Buffer.byteLength(JSON.stringify({ message: "récolté" })));
  const combined = Buffer.concat([first, encode(1, "refusé")]);
  const decoder = new FrameDecoder();
  const frames = [];
  for (const byte of combined) frames.push(...decoder.push(Buffer.from([byte])));
  assert.equal(frames.length, 2);
  assert.deepEqual(JSON.parse(frames[0]!.payload), { message: "récolté" });
  assert.equal(frames[1]!.type, 1);
  const bad = Buffer.alloc(6);
  bad.writeUInt32BE(2 * 1024 * 1024, 2);
  assert.throws(() => new FrameDecoder().push(bad), /1 MiB/);
});

test("plans restrict actions and arguments", () => {
  const decision = parseDecision(plan([["MOVE", "W"], ["TAKE", "pomme"], ["ATTACK", "#7"], ["LOOK", "north"]]));
  assert.deepEqual(gameCommand(decision.plan[0]!, 12), { type: 1, payload: { agentID: 12, direction: 270 } });
  assert.deepEqual(gameCommand(decision.plan[1]!, 12), { type: 3, payload: { agentID: 12, type: 0 } });
  assert.deepEqual(gameCommand(decision.plan[2]!, 12), { type: 5, payload: { agentID: 12, targetId: 7 } });
  assert.deepEqual(gameCommand(decision.plan[3]!, 12), { type: 2, payload: { agentID: 12, direction: 0 } });
  assert.equal(gameCommand({ action: "TAKE", arg: "w" }, 12).payload.type, 3);
  assert.throws(() => parseDecision(plan([["MOVE", "UP"]])), /direction/);
  assert.throws(() => parseDecision(plan([["TAKE", "gold"]])), /resource/);
  assert.throws(() => parseDecision(plan([["ATTACK", "0"]])), /target/);
  assert.throws(() => parseDecision(plan([])), /at least one/);
  assert.throws(() => parseDecision(JSON.stringify({ plan: [{ action: "DROP", arg: "" }], memory: "", why: "" })), /Unsupported/);
  assert.equal(parseDecision(plan(Array.from({ length: 12 }, (): [Step["action"], string] => ["MOVE", "N"]))).plan.length, 8);
  assert.equal(parseDecision(plan([["STOP"]], "x".repeat(900))).memory.length, 200);
});

test("SDK uses API key authentication, isolated context and read-only permissions", () => {
  const options = sdkOptions("test-secret", "/session", { PATH: "runtime", OPENAI_API_KEY: "do-not-inherit", CODEX_HOME: "/personal", GITHUB_TOKEN: "private" });
  assert.equal(options.apiKey, "test-secret");
  assert.equal(options.env?.PATH, "runtime");
  assert.equal(options.env?.OPENAI_API_KEY, undefined);
  assert.equal(options.env?.GITHUB_TOKEN, undefined);
  assert.equal(options.env?.CODEX_HOME, join("/session", "codex-home"));
  assert.equal(options.config?.forced_login_method, "api");
  assert.equal(threadOptions("/session").sandboxMode, "read-only");
  assert.equal(threadOptions("/session").approvalPolicy, "never");
  assert.equal(threadOptions("/session").networkAccessEnabled, false);
  assert.equal(threadOptions("/session").webSearchMode, "disabled");
  assert.equal(threadOptions("/session").model, undefined);
});

test("config resolves Claude model aliases, effort and verbosity", () => {
  assert.equal(readConfig(["--port", "1234", "--max-decisions", "3"], {}).port, 1234);
  assert.throws(() => readConfig(["--max-decisions", "0"], {}), /between/);
  assert.throws(() => readConfig(["--api-key", "secret"], {}));
  assert.equal(redact("token test-secret and sk-abcdefghi", "test-secret"), "token [REDACTED] and [REDACTED]");
  assert.equal(readConfig([], {}).provider, "codex");
  assert.equal(readConfig([], {}).claudeModel, null);
  assert.equal(readConfig(["--provider", "claude"], {}).claudeModel?.id, "claude-opus-5-5");
  assert.equal(readConfig(["--provider", "claude", "--model", "haiku"], {}).claudeModel?.id, "claude-haiku-5-5");
  assert.equal(readConfig(["--provider", "claude", "-m", "Sonnet"], {}).claudeModel?.id, "claude-sonnet-5-5");
  assert.equal(readConfig(["--provider", "claude"], { CLAUDE_MODEL: "fable" }).claudeModel?.id, "claude-fable-5-1");
  assert.equal(readConfig(["--provider", "claude", "--model", "claude-opus-5"], {}).claudeModel?.pricing, null);
  assert.throws(() => readConfig(["--provider", "claude", "--model", "gpt"], {}), /model/);
  assert.equal(readConfig(["--provider", "codex"], { ZIOOP_PROVIDER: "claude", CODEX_MODEL: "codex-model" }).model, "codex-model");
  assert.equal(readConfig([], {}).effort, "low");
  assert.equal(readConfig(["--effort", "HIGH"], {}).effort, "high");
  assert.throws(() => readConfig(["--effort", "turbo"], {}), /effort/);
  assert.equal(readConfig(["-v"], {}).verbose, true);
  assert.throws(() => readConfig(["--provider", "unknown"], {}), /provider/);
});

test("history paginates older events and persists actual replies", async (t) => {
  const path = join(await directory(t), "history.jsonl");
  const journal = new Journal(path);
  for (let i = 0; i < 25; ++i) await journal.append({ command: "MOVE", parameters: { direction: 90 }, reason: "Explore", response: { ok: i % 2 === 0, data: i }, durationMs: 2 });
  const last = journal.page(null, 5);
  assert.deepEqual(last.entries.map((entry) => entry.sequence), [21, 22, 23, 24, 25]);
  assert.equal(last.nextBefore, 21);
  assert.equal((await readJournal(path))[1]!.response.ok, false);
});

test("state follows server replies, rituals and events", () => {
  const state = initialState({ ok: true, data: { agentId: 12, position: { x: 2, y: 3 } } });
  assert.equal(applyReply(state, "MOVE", "E", { ok: false, data: "Unable to move agent" }).interrupt, "MOVE refusé");
  assert.equal(state.maxX, 2);
  applyReply(state, "MOVE", "W", { ok: true, data: { agentId: 12, position: { x: 1, y: 3 } } });
  assert.equal(state.x, 1);
  assert.equal(state.visited.has("1,3"), true);
  applyReply(state, "TAKE", "a", { ok: true, data: { resources: [["agentId", 12], { type: 0, quantity: 10 }, { type: 1, quantity: 5 }, { type: 2, quantity: 2 }, { type: 3, quantity: 1 }] } });
  assert.deepEqual(state.inventory, [10, 5, 2, 1]);
  assert.equal(ritualReady(state), true);
  applyReply(state, "RITUAL", "", { ok: true, data: { agentId: 12, status: 0 } });
  assert.deepEqual(state.inventory, [0, 0, 0, 0]);
  assert.equal(state.ritualLevel, 2);
  assert.equal(applyEvent(state, { type: "Ritual", agentId: 12, level: 2 }).ritualDone, true);
  assert.equal(state.level, 2);
  assert.deepEqual(missing(state), [20, 10, 4, 2]);
  assert.equal(ritualTimeout(2), 105_000);
  state.inventory = [3, 0, 1, 0];
  applyEvent(state, { type: "RottenResources", agentId: 12, resources: [2, 0] });
  assert.deepEqual(state.inventory, [2, 0, 0, 0]);
  assert.equal(applyEvent(state, { type: "Died", agentId: 99 }).died, false);
  assert.equal(applyEvent(state, { type: "Message", agentId: 12, speakerId: 4, message: "salut", direction: 0 }).interrupt, "message reçu");
  assert.equal(applyEvent(state, { type: "Died", agentId: 12 }).died, true);
  assert.equal(state.alive, false);
});

test("LOOK feeds known resources, agents and the compact prompt", () => {
  const state = initialState({ ok: true, data: { agentId: 12, position: { x: 2, y: 2 } } });
  const outcome = applyReply(state, "LOOK", "N", { ok: true, data: { agentId: 12, cells: [
    { position: [2, 2], resources: [{ type: 0 }, { type: 2 }, { type: 0 }], agents: [{ id: 7, level: 1, health: 90 }, { id: 12, level: 1, health: 100 }] },
    { position: [2, 1], resources: [{ type: 1 }] },
  ] } });
  assert.match(outcome.interrupt!, /#7/);
  assert.deepEqual(wantedHere(state), [0, 2, 0]);
  assert.equal(state.agents.has(12), false);
  applyReply(state, "ATTACK", "7", { ok: true, data: { agentId: 12, targetId: 7, killed: true } });
  assert.equal(state.kills, 1);
  const prompt = renderState(state, 5, "test");
  assert.match(prompt, /pos=2,2 niv=1/);
  assert.match(prompt, /2,1:c/);
  assert.match(prompt, /@/);
  assert.ok(prompt.length < 1500, `prompt too long: ${prompt.length}`);
});

test("KO replies expose the server message as text", async (t) => {
  const replies = [encode(1, { agentId: 1, message: "Target is too far from you" }), encode(1, { reason: "Old reason" }), rawFrame(1, "Invalid payload")];
  const connection = await fixture(t, (type, _data, socket) => {
    if (type !== Message.DISCONNECT) socket.write(replies.shift()!);
  });
  const signal = new AbortController().signal;
  assert.deepEqual(await connection.request(Message.ATTACK, { agentID: 1, targetId: 2 }, 500, signal), { ok: false, data: "Target is too far from you" });
  assert.deepEqual(await connection.request(Message.ATTACK, { agentID: 1, targetId: 2 }, 500, signal), { ok: false, data: "Old reason" });
  assert.deepEqual(await connection.request(Message.ATTACK, { agentID: 1, targetId: 2 }, 500, signal), { ok: false, data: "Invalid payload" });
});

test("connection serializes requests, receives a delayed response and forwards events", async (t) => {
  const connection = await fixture(t, (type, _data, socket) => {
    if (type === Message.DISCONNECT) return;
    setTimeout(() => {
      socket.write(encode(5, { type: "Message", agentId: 1, message: "hello" }));
      const frame = encode(0, { agentId: 1, status: 0 });
      socket.write(frame.subarray(0, 3));
      socket.write(frame.subarray(3));
    }, 30);
  });
  const events: unknown[] = [];
  connection.onEvent = (event) => events.push(event);
  const signal = new AbortController().signal;
  const pending = connection.request(Message.RITUAL, { agentID: 1 }, 500, signal);
  await assert.rejects(connection.request(Message.MOVE, { agentID: 1 }, 500, signal), /Only one/);
  assert.deepEqual(await pending, { ok: true, data: { agentId: 1, status: 0 } });
  assert.deepEqual(events, [{ type: "Message", agentId: 1, message: "hello" }]);
});

test("timeout closes the connection instead of replaying an uncertain command", async (t) => {
  const connection = await fixture(t, () => {});
  const signal = new AbortController().signal;
  await assert.rejects(connection.request(Message.TAKE, { agentID: 1, type: 0 }, 20, signal), /unknown/);
  await assert.rejects(connection.request(Message.TAKE, { agentID: 1, type: 0 }, 20, signal), /unknown/);
});

test("Ctrl+C cancels a pending command", async (t) => {
  const connection = await fixture(t, () => {});
  const controller = new AbortController();
  const pending = connection.request(Message.RITUAL, { agentID: 1 }, 75_000, controller.signal);
  controller.abort();
  await assert.rejects(pending, /cancelled/);
});

test("plans run several actions per AI call, auto-take and auto-ritual", async (t) => {
  const sent: number[] = [];
  let x = 2;
  let ritual = false;
  const connection = await fixture(t, (type, data, socket) => {
    if (type === Message.DISCONNECT) return;
    assert.equal(ritual, false, "No command should overlap the ritual");
    sent.push(type);
    if (type === Message.HELLO) socket.write(encode(0, { agentId: 12, position: { x, y: 2 } }));
    if (type === Message.MOVE) {
      x += 1;
      socket.write(encode(0, { agentId: 12, position: { x, y: 2 } }));
    }
    if (type === Message.LOOK) socket.write(encode(0, { agentId: 12, cells: [{ position: [x, 2], resources: x === 4 ? [{ type: 0 }] : [] }] }));
    if (type === Message.TAKE) {
      assert.equal(data.type, 0);
      socket.write(encode(0, { resources: [{ type: 0, quantity: 10 }, { type: 1, quantity: 5 }, { type: 2, quantity: 2 }, { type: 3, quantity: 1 }] }));
    }
    if (type === Message.RITUAL) {
      ritual = true;
      socket.write(encode(0, { agentId: 12, status: 0 }));
      setTimeout(() => {
        ritual = false;
        socket.write(encode(5, { type: "Ritual", agentId: 12, level: 2 }));
      }, 40);
    }
  });
  const prompts: string[] = [];
  const runDirectory = await directory(t);
  const journal = new Journal(join(runDirectory, "history.jsonl"));
  const state = await runAgent({ connection, brain: brain([plan([["MOVE", "E"], ["MOVE", "E"], ["MOVE", "E"]])], prompts), journal, runDirectory,
    maxDecisions: 5, signal: new AbortController().signal, log: () => {} });
  assert.equal(state.level, 2);
  assert.deepEqual(sent, [Message.HELLO, Message.LOOK, Message.MOVE, Message.LOOK, Message.MOVE, Message.LOOK, Message.TAKE, Message.RITUAL]);
  assert.equal(prompts.length, 2);
  assert.match(prompts[1]!, /rituel terminé, niveau 2/);
  assert.match(prompts[1]!, /Remember the fruit/);
  const session = JSON.parse(await readFile(join(runDirectory, "session.json"), "utf8"));
  assert.equal(session.status, "stopped");
  assert.equal(session.metrics.calls, 2);
  assert.equal(session.metrics.actions, 7);
  assert.equal(session.metrics.usage.cacheRead, 200);
  assert.ok((await readJournal(journal.path)).some((entry) => entry.command === "DECISION"));
});

test("a KO interrupts the plan and the agent stops when it dies", async (t) => {
  const sent: number[] = [];
  const connection = await fixture(t, (type, _data, socket) => {
    if (type === Message.DISCONNECT) return;
    sent.push(type);
    if (type === Message.HELLO) socket.write(encode(0, { agentId: 12, position: { x: 0, y: 0 } }));
    if (type === Message.LOOK) socket.write(encode(0, { agentId: 12, cells: [{ position: [0, 0], resources: [] }] }));
    if (type === Message.MOVE) {
      socket.write(encode(5, { type: "Died", agentId: 12 }));
      socket.write(encode(1, { agentId: 12, message: "Unable to move agent" }));
    }
  });
  const prompts: string[] = [];
  const runDirectory = await directory(t);
  const state = await runAgent({ connection, brain: brain([plan([["MOVE", "N"], ["MOVE", "N"]])], prompts), journal: new Journal(join(runDirectory, "history.jsonl")),
    runDirectory, maxDecisions: 5, signal: new AbortController().signal, log: () => {} });
  assert.equal(state.alive, false);
  assert.deepEqual(sent, [Message.HELLO, Message.LOOK, Message.MOVE]);
  assert.equal(prompts.length, 1);
});

test("three invalid model decisions stop the client", async (t) => {
  const sent: number[] = [];
  const connection = await fixture(t, (type, _data, socket) => {
    if (type === Message.DISCONNECT) return;
    sent.push(type);
    if (type === Message.HELLO) socket.write(encode(0, { agentId: 1, position: { x: 0, y: 0 } }));
    if (type === Message.LOOK) socket.write(encode(0, { agentId: 1, cells: [] }));
  });
  const runDirectory = await directory(t);
  await assert.rejects(runAgent({ connection, brain: brain(["not json", "{}", "[]"]), journal: new Journal(join(runDirectory, "history.jsonl")),
    runDirectory, maxDecisions: 10, signal: new AbortController().signal, log: () => {} }), /3 invalid/);
  assert.deepEqual(sent, [Message.HELLO, Message.LOOK]);
  await delay(1);
});
