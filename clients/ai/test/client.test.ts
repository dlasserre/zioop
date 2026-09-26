import assert from "node:assert/strict";
import { test, type TestContext } from "node:test";
import { createServer, type Socket } from "node:net";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { setTimeout as delay } from "node:timers/promises";
import { FrameDecoder, GameConnection, Message, encode } from "../src/protocol.js";
import { gameCommand, parseDecision, type Decision } from "../src/commands.js";
import { Journal, readJournal } from "../src/history.js";
import { initialState, responseTimeout, runAgent, updateState } from "../src/agent.js";
import { sdkOptions, threadOptions } from "../src/brain.js";
import { readConfig, redact } from "../src/config.js";

function decision(command: Decision["command"], extra: Partial<Decision> = {}): string {
  return JSON.stringify({ command, direction: null, target: null, historyBefore: null, historyLimit: null, reason: "Test action", memory: "Remember the fruit", ...extra });
}

// The real server writes KO reasons as raw text, not as a JSON string.
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

test("decisions restrict commands, parameters and agent identity", () => {
  const move = parseDecision(decision("MOVE", { direction: "WEST" }));
  assert.deepEqual(gameCommand(move, 12), { type: 1, payload: { agentID: 12, direction: 270 } });
  assert.deepEqual(gameCommand(parseDecision(decision("TAKE")), 12), { type: 4, payload: { agentID: 12, type: 0 } });
  assert.throws(() => parseDecision(decision("MOVE")), /direction/);
  assert.throws(() => parseDecision(decision("RITUAL", { direction: "NORTH" })), /direction/);
  assert.throws(() => parseDecision(decision("HISTORY", { historyLimit: 21 })), /limit/);
  assert.throws(() => parseDecision(decision("STOP").replace("STOP", "DROP")), /Unsupported/);
  assert.throws(() => parseDecision(JSON.stringify({ ...JSON.parse(decision("TAKE")), agentID: 99 })), /fields/);
  assert.deepEqual(gameCommand(parseDecision(decision("ATTACK", { target: 7 })), 12), { type: 6, payload: { agentID: 12, targetId: 7 } });
  assert.throws(() => parseDecision(decision("ATTACK")), /target/);
  assert.throws(() => parseDecision(decision("ATTACK", { target: 0 })), /target/);
  assert.throws(() => parseDecision(decision("ATTACK", { target: 7, direction: "NORTH" })), /direction/);
  assert.throws(() => parseDecision(decision("MOVE", { direction: "EAST", target: 7 })), /target/);
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

test("config and redaction never need a key on the command line", () => {
  assert.equal(readConfig(["--port", "1234", "--max-decisions", "3"], {}).port, 1234);
  assert.throws(() => readConfig(["--max-decisions", "0"], {}), /between/);
  assert.throws(() => readConfig(["--api-key", "secret"], {}));
  assert.equal(redact("token test-secret and sk-abcdefghi", "test-secret"), "token [REDACTED] and [REDACTED]");
  assert.equal(readConfig([], {}).provider, "codex");
  assert.equal(readConfig(["--provider", "claude"], { CODEX_MODEL: "codex-model", CLAUDE_MODEL: "claude-model" }).model, "claude-model");
  assert.equal(readConfig(["--provider", "codex"], { ZIOOP_PROVIDER: "claude", CODEX_MODEL: "codex-model" }).model, "codex-model");
  assert.throws(() => readConfig(["--provider", "unknown"], {}), /provider/);
});

test("history paginates older events and persists actual replies", async (t) => {
  const path = join(await directory(t), "history.jsonl");
  const journal = new Journal(path);
  for (let i = 0; i < 25; ++i) await journal.append({ command: "MOVE", parameters: { direction: 90 }, reason: "Explore", response: { ok: i % 2 === 0, data: i }, durationMs: 2 });
  const last = journal.page(null, 5);
  assert.deepEqual(last.entries.map((entry) => entry.sequence), [21, 22, 23, 24, 25]);
  assert.equal(last.nextBefore, 21);
  assert.deepEqual(journal.page(last.nextBefore, 3).entries.map((entry) => entry.sequence), [18, 19, 20]);
  assert.equal((await readJournal(path))[1]!.response.ok, false);
});

test("state uses successful server results and allows full ritual durations", () => {
  const state = initialState({ ok: true, data: { agentID: 12, position: { x: 2, y: 3 } } });
  updateState(state, "MOVE", { ok: false, data: "Blocked" });
  assert.deepEqual(state.position, { x: 2, y: 3 });
  updateState(state, "MOVE", { ok: true, data: { position: { x: 1, y: 3 } } });
  assert.equal(state.visited["1,3"], 1);
  updateState(state, "RITUAL", { ok: true, data: { agentID: 12, level: 2 } });
  assert.equal(state.level, 2);
  assert.equal(state.inventory, null);
  assert.equal(responseTimeout("RITUAL", 2), 105_000);
  assert.throws(() => updateState(state, "RITUAL", { ok: true, data: { agentID: 99, level: 3 } }), /another agent/);
});

test("state remembers agents seen by LOOK and forgets eliminated targets", () => {
  const state = initialState({ ok: true, data: { agentID: 12, position: { x: 2, y: 2 } } });
  updateState(state, "LOOK", { ok: true, data: { cells: [
    { position: [2, 2], resources: [], agents: [{ id: 7, level: 1 }, { id: 9, level: 3 }] },
    { position: [2, 1], resources: [{ type: 0, rotting: 0 }] },
  ] } });
  assert.deepEqual(state.agents, { 7: { level: 1, position: [2, 2] }, 9: { level: 3, position: [2, 2] } });
  // Agent 9 is no longer on the visible cell.
  updateState(state, "LOOK", { ok: true, data: { cells: [{ position: [2, 2], resources: [], agents: [{ id: 7, level: 1 }] }] } });
  assert.deepEqual(Object.keys(state.agents), ["7"]);
  updateState(state, "ATTACK", { ok: false, data: "Target is too strong for you" });
  assert.equal(state.kills, 0);
  updateState(state, "ATTACK", { ok: true, data: { targetId: 7, killed: 1 } });
  assert.deepEqual(state.agents, {});
  assert.equal(state.kills, 1);
  assert.throws(() => updateState(state, "LOOK", { ok: true, data: { cells: [{ position: [2, 2], agents: {} }] } }), /agents/);
});

test("KO replies expose the server reason as text", async (t) => {
  const replies = [encode(1, { reason: "Target is too far from you" }), rawFrame(1, "Invalid payload")];
  const connection = await fixture(t, (type, _data, socket) => {
    if (type !== Message.DISCONNECT) socket.write(replies.shift()!);
  });
  const signal = new AbortController().signal;
  assert.deepEqual(await connection.request(Message.ATTACK, { agentID: 1, targetId: 2 }, 500, signal), { ok: false, data: "Target is too far from you" });
  assert.deepEqual(await connection.request(Message.ATTACK, { agentID: 1, targetId: 2 }, 500, signal), { ok: false, data: "Invalid payload" });
});

test("connection serializes requests and receives a delayed response", async (t) => {
  const connection = await fixture(t, (type, _data, socket) => {
    if (type === Message.DISCONNECT) return;
    setTimeout(() => {
      const frame = encode(0, { agentID: 1, level: 2 });
      socket.write(frame.subarray(0, 3));
      socket.write(frame.subarray(3));
    }, 30);
  });
  const signal = new AbortController().signal;
  const pending = connection.request(Message.RITUAL, { agentID: 1 }, 500, signal);
  await assert.rejects(connection.request(Message.MOVE, { agentID: 1 }, 500, signal), /Only one/);
  assert.deepEqual(await pending, { ok: true, data: { agentID: 1, level: 2 } });
});

test("timeout closes the connection instead of replaying an uncertain command", async (t) => {
  const connection = await fixture(t, () => {});
  const signal = new AbortController().signal;
  await assert.rejects(connection.request(Message.TAKE, { agentID: 1, type: 0 }, 20, signal), /unknown/);
  await assert.rejects(connection.request(Message.TAKE, { agentID: 1, type: 0 }, 20, signal), /unknown/);
});

test("Ctrl+C cancels a ritual without waiting for its deadline", async (t) => {
  const connection = await fixture(t, () => {});
  const controller = new AbortController();
  const pending = connection.request(Message.RITUAL, { agentID: 1 }, 75_000, controller.signal);
  controller.abort();
  await assert.rejects(pending, /cancelled/);
});

for (const provider of ["Codex", "Claude"]) test(`${provider} game loop retains context, consults history and waits for ritual completion`, async (t) => {
  const sent: number[] = [];
  let ritualPending = false;
  const connection = await fixture(t, (type, _data, socket) => {
    if (type === Message.DISCONNECT) return;
    assert.equal(ritualPending, false, "No command should overlap the ritual");
    sent.push(type);
    if (type === Message.HELLO) socket.write(encode(0, { agentID: 12, position: { x: 2, y: 2 } }));
    if (type === Message.LOOK) socket.write(encode(0, { cells: [{ position: [2, 2], resources: [{ type: 0, rotting: 0 }] }] }));
    if (type === Message.TAKE) socket.write(encode(0, { resources: [{ type: 0, quantity: 10 }] }));
    if (type === Message.RITUAL) {
      ritualPending = true;
      setTimeout(() => { ritualPending = false; socket.write(encode(0, { agentID: 12, level: 2 })); }, 40);
    }
  });
  const prompts: string[] = [];
  const decisions = [decision("LOOK", { direction: "WEST" }), decision("TAKE"), decision("RITUAL"), decision("HISTORY", { historyBefore: 4, historyLimit: 2 }), decision("STOP")];
  const runDirectory = await directory(t);
  const journal = new Journal(join(runDirectory, "history.jsonl"));
  const state = await runAgent({ connection, brain: { name: provider, threadId: "test-thread", async decide(prompt) { prompts.push(prompt); return decisions.shift()!; } },
    journal, runDirectory, maxDecisions: 10, signal: new AbortController().signal, log: () => {} });
  assert.equal(state.level, 2);
  assert.deepEqual(sent, [Message.HELLO, Message.LOOK, Message.TAKE, Message.RITUAL]);
  assert.match(prompts[1]!, /rotting/);
  assert.match(prompts[4]!, /local_history/);
  assert.match(prompts[4]!, /Remember the fruit/);
  const history = await readJournal(journal.path);
  assert.deepEqual(history.find((entry) => entry.command === "HISTORY")!.response.data, { sequences: [2, 3], nextBefore: 2 });
  const session = JSON.parse(await readFile(join(runDirectory, "session.json"), "utf8"));
  assert.equal(session.threadId, "test-thread");
  assert.equal(session.provider, provider);
  assert.equal(session.status, "stopped");
});

test("game loop attacks an agent seen on its cell", async (t) => {
  const attacks: Record<string, unknown>[] = [];
  const connection = await fixture(t, (type, data, socket) => {
    if (type === Message.HELLO) socket.write(encode(0, { agentID: 12, position: { x: 2, y: 2 } }));
    if (type === Message.LOOK) socket.write(encode(0, { cells: [{ position: [2, 2], resources: [], agents: [{ id: 7, level: 1 }] }] }));
    if (type === Message.ATTACK) {
      attacks.push(data);
      socket.write(encode(0, { targetId: 7, killed: 1 }));
    }
  });
  const prompts: string[] = [];
  const decisions = [decision("LOOK", { direction: "NORTH" }), decision("ATTACK", { target: 7 }), decision("STOP")];
  const runDirectory = await directory(t);
  const state = await runAgent({ connection, brain: { name: "Codex", threadId: null, async decide(prompt) { prompts.push(prompt); return decisions.shift()!; } },
    journal: new Journal(join(runDirectory, "history.jsonl")), runDirectory, maxDecisions: 10, signal: new AbortController().signal, log: () => {} });
  assert.deepEqual(attacks, [{ agentID: 12, targetId: 7 }]);
  assert.match(prompts[1]!, /"agents":\{"7":\{"level":1,"position":\[2,2\]\}\}/);
  assert.equal(state.kills, 1);
  assert.deepEqual(state.agents, {});
});

test("three invalid model decisions stop without sending an action", async (t) => {
  const sent: number[] = [];
  const connection = await fixture(t, (type, _data, socket) => {
    if (type === Message.DISCONNECT) return;
    sent.push(type);
    socket.write(encode(0, { agentID: 1, position: { x: 0, y: 0 } }));
  });
  const runDirectory = await directory(t);
  await assert.rejects(runAgent({ connection, brain: { name: "Codex", threadId: null, async decide() { return "not json"; } }, journal: new Journal(join(runDirectory, "history.jsonl")),
    runDirectory, maxDecisions: 10, signal: new AbortController().signal, log: () => {} }), /three invalid/);
  assert.deepEqual(sent, [Message.HELLO]);
  await delay(1);
});
