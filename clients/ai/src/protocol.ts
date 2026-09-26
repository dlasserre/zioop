import { Socket } from "node:net";

// Protocol.hpp: uint16 type + uint32 UTF-8 payload length, both in network order.
export const Message = { HELLO: 0, MOVE: 1, LOOK: 3, TAKE: 4, ATTACK: 6, INVENTORY: 8, RITUAL: 9, DISCONNECT: 10 } as const;
export const Direction = { NORTH: 0, SOUTH: 90, EAST: 180, WEST: 270 } as const;
export type GameReply = { ok: boolean; data: unknown };
const MAX_PAYLOAD = 1024 * 1024;

// KO payloads are plain text, or JSON carrying a "reason" (ATTACK).
function failureReason(payload: string): string {
  try {
    const value: unknown = JSON.parse(payload);
    if (value && typeof value === "object" && "reason" in value && typeof value.reason === "string") return value.reason;
  } catch { /* plain-text reason */ }
  return payload;
}

export function encode(type: number, payload: unknown): Buffer {
  const body = Buffer.from(JSON.stringify(payload), "utf8");
  if (body.length > MAX_PAYLOAD) throw new Error("Payload exceeds 1 MiB");
  const header = Buffer.alloc(6);
  header.writeUInt16BE(type, 0);
  header.writeUInt32BE(body.length, 2);
  return Buffer.concat([header, body]);
}

export class FrameDecoder {
  private buffer: Buffer = Buffer.alloc(0);
  push(chunk: Buffer): { type: number; payload: string }[] {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    const frames: { type: number; payload: string }[] = [];
    while (this.buffer.length >= 6) {
      const size = this.buffer.readUInt32BE(2);
      if (size > MAX_PAYLOAD) throw new Error("Server frame exceeds 1 MiB");
      if (this.buffer.length < size + 6) break;
      frames.push({ type: this.buffer.readUInt16BE(0), payload: this.buffer.subarray(6, size + 6).toString("utf8") });
      this.buffer = this.buffer.subarray(size + 6);
    }
    return frames;
  }
}

export class GameConnection {
  private readonly socket = new Socket();
  private readonly decoder = new FrameDecoder();
  private pending: { resolve: (reply: GameReply) => void; reject: (error: Error) => void; cleanup: () => void } | undefined;
  private failure: Error | undefined;
  private connected = false;
  onDisconnect: (() => void) | undefined;

  constructor() {
    this.socket.on("error", (error) => this.fail(error));
    this.socket.on("close", () => this.fail(new Error("Game connection closed")));
    this.socket.on("data", (chunk: Buffer) => {
      try {
        for (const frame of this.decoder.push(chunk)) {
          if (!this.pending) throw new Error("Unexpected server response; cannot correlate commands");
          if (frame.type !== 0 && frame.type !== 1) throw new Error(`Unknown response type ${frame.type}`);
          const reply: GameReply = { ok: frame.type === 0, data: frame.type === 0 ? JSON.parse(frame.payload) : failureReason(frame.payload) };
          const pending = this.pending;
          this.pending = undefined;
          pending.cleanup();
          pending.resolve(reply);
        }
      } catch (error) {
        this.fail(error instanceof Error ? error : new Error(String(error)));
      }
    });
  }

  async connect(host: string, port: number, signal: AbortSignal): Promise<void> {
    signal.throwIfAborted();
    await new Promise<void>((resolve, reject) => {
      const timeout = setTimeout(() => this.fail(new Error("Game connection timed out")), 10_000);
      const abort = () => this.fail(new Error("Connection cancelled"));
      const cleanup = () => {
        clearTimeout(timeout);
        signal.removeEventListener("abort", abort);
        this.socket.off("connect", onConnect);
        this.socket.off("close", onClose);
      };
      const onConnect = () => { cleanup(); this.connected = true; resolve(); };
      const onClose = () => { cleanup(); reject(this.failure ?? new Error("Connection closed")); };
      this.socket.once("connect", onConnect);
      this.socket.once("close", onClose);
      signal.addEventListener("abort", abort, { once: true });
      this.socket.connect(port, host);
    });
  }

  async request(type: number, payload: unknown, timeoutMs: number, signal: AbortSignal): Promise<GameReply> {
    signal.throwIfAborted();
    if (this.failure) throw this.failure;
    if (!this.connected) throw new Error("Not connected to Zioop");
    if (this.pending) throw new Error("Only one game command may be pending");
    const frame = encode(type, payload);
    return new Promise<GameReply>((resolve, reject) => {
      // A timeout closes the connection: replaying a TAKE/RITUAL could apply it twice.
      const timeout = setTimeout(() => this.fail(new Error("Game response timed out; command outcome is unknown")), timeoutMs);
      const abort = () => this.fail(new Error("Game command cancelled"));
      this.pending = { resolve, reject, cleanup: () => {
        clearTimeout(timeout);
        signal.removeEventListener("abort", abort);
      } };
      signal.addEventListener("abort", abort, { once: true });
      this.socket.write(frame);
    });
  }

  close(): void {
    if (this.connected && !this.failure) this.socket.write(encode(Message.DISCONNECT, {}));
    this.fail(new Error("Game connection stopped"));
  }

  private fail(error: Error): void {
    if (this.failure) return;
    this.failure = error;
    this.connected = false;
    const pending = this.pending;
    this.pending = undefined;
    pending?.cleanup();
    pending?.reject(error);
    this.socket.destroy();
    this.onDisconnect?.();
  }
}
