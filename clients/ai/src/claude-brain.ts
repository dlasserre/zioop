import { mkdir } from "node:fs/promises";
import { join } from "node:path";
import { query, type Options, type SDKMessage } from "@anthropic-ai/claude-agent-sdk";
import { runtimeEnvironment, type Brain } from "./brain.js";
import { decisionSchema, GAME_INSTRUCTIONS } from "./commands.js";

export type ClaudeQueryFactory = (args: { prompt: string; options: Options }) => AsyncIterable<SDKMessage> & { close(): void };

export function claudeOptions(apiKey: string, runDirectory: string, environment: NodeJS.ProcessEnv, model?: string): Options {
  return {
    ...(model ? { model } : {}),
    cwd: join(runDirectory, "context"),
    env: { ...runtimeEnvironment(environment), ANTHROPIC_API_KEY: apiKey, CLAUDE_CONFIG_DIR: join(runDirectory, "claude-home") },
    systemPrompt: GAME_INSTRUCTIONS,
    tools: [],
    settingSources: [],
    plugins: [],
    skills: [],
    mcpServers: {},
    strictMcpConfig: true,
    permissionMode: "dontAsk",
    persistSession: true,
    outputFormat: { type: "json_schema", schema: decisionSchema },
    maxTurns: 5,
  };
}

export async function createClaudeBrain(apiKey: string, runDirectory: string, model?: string, runQuery: ClaudeQueryFactory = query): Promise<Brain> {
  await mkdir(join(runDirectory, "claude-home"), { recursive: true });
  await mkdir(join(runDirectory, "context"), { recursive: true });
  const options = claudeOptions(apiKey, runDirectory, process.env, model);
  let sessionId: string | null = null;
  return {
    name: "Claude",
    get threadId() { return sessionId; },
    async decide(prompt, signal) {
      signal.throwIfAborted();
      const abortController = new AbortController();
      const cancel = () => abortController.abort(signal.reason);
      signal.addEventListener("abort", cancel, { once: true });
      let stream: ReturnType<ClaudeQueryFactory> | undefined;
      try {
        stream = runQuery({ prompt, options: { ...options, abortController, ...(sessionId ? { resume: sessionId } : {}) } });
        let output: string | undefined;
        for await (const message of stream) {
          if (message.type === "system" && message.subtype === "init") sessionId = message.session_id;
          if (message.type === "result") {
            sessionId = message.session_id;
            if (message.subtype !== "success") throw new Error(`Claude: ${message.subtype}: ${message.errors.join("; ")}`);
            if (message.is_error) throw new Error(`Claude: ${message.result || "API request failed"}`);
            if (message.structured_output === undefined) throw new Error("Claude returned no valid structured decision");
            output = JSON.stringify(message.structured_output);
          }
        }
        if (output === undefined) throw new Error("Claude query ended without a decision");
        return output;
      } finally {
        signal.removeEventListener("abort", cancel);
        stream?.close();
      }
    },
  };
}
