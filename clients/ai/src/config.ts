import { parseArgs } from "node:util";
import { resolve } from "node:path";

export function readConfig(args: string[], env: NodeJS.ProcessEnv) {
  const { values } = parseArgs({ args, options: {
    provider: { type: "string" },
    host: { type: "string" }, port: { type: "string" }, model: { type: "string" },
    "max-decisions": { type: "string" }, "log-dir": { type: "string" }, help: { type: "boolean", short: "h" },
  } });
  const provider = values.provider ?? env.ZIOOP_PROVIDER ?? "codex";
  if (provider !== "codex" && provider !== "claude") throw new Error("provider must be codex or claude");
  function number(value: string, name: string, max: number): number {
    if (!/^\d+$/.test(value)) throw new Error(`${name} must be a positive integer`);
    const result = Number(value);
    if (!Number.isSafeInteger(result) || result < 1 || result > max) throw new Error(`${name} must be between 1 and ${max}`);
    return result;
  }
  return {
    help: values.help ?? false,
    provider,
    host: values.host ?? env.ZIOOP_HOST ?? "127.0.0.1",
    port: number(values.port ?? env.ZIOOP_PORT ?? "9090", "port", 65535),
    model: (values.model ?? (provider === "codex" ? env.CODEX_MODEL : env.CLAUDE_MODEL)?.trim()) || undefined,
    maxDecisions: number(values["max-decisions"] ?? env.ZIOOP_MAX_DECISIONS ?? "100", "max-decisions", 10_000),
    logDirectory: resolve(values["log-dir"] ?? "runs"),
  };
}

export function redact(message: string, secret: string): string {
  return (secret ? message.split(secret).join("[REDACTED]") : message).replace(/sk-[A-Za-z0-9_-]{8,}/g, "[REDACTED]");
}
