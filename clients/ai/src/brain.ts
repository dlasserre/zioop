import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { Codex, type CodexOptions, type ThreadOptions } from "@openai/codex-sdk";
import { decisionSchema, GAME_INSTRUCTIONS } from "./commands.js";
import type { Usage } from "./metrics.js";
import type { Pricing } from "./models.js";

export type BrainReply = { text: string; usage: Usage | null };

export interface Brain {
  readonly name: string;
  readonly model: string;
  readonly pricing: Pricing | null;
  decide(prompt: string, signal: AbortSignal): Promise<BrainReply>;
}

export function runtimeEnvironment(environment: NodeJS.ProcessEnv): Record<string, string> {
  const env: Record<string, string> = {};
  // Keep OS/runtime essentials, excluding unrelated credentials and inherited Codex settings.
  const allowed = new Set(["PATH", "SYSTEMROOT", "WINDIR", "COMSPEC", "PATHEXT", "TEMP", "TMP", "USERPROFILE", "HOME", "LOCALAPPDATA", "APPDATA", "SSL_CERT_FILE", "SSL_CERT_DIR", "HTTP_PROXY", "HTTPS_PROXY", "NO_PROXY"]);
  for (const [name, value] of Object.entries(environment)) {
    if (value !== undefined && allowed.has(name.toUpperCase())) env[name] = value;
  }
  return env;
}

export function sdkOptions(apiKey: string, runDirectory: string, environment: NodeJS.ProcessEnv): CodexOptions {
  const env = runtimeEnvironment(environment);
  // A dedicated Codex home prevents inheriting personal logins, plugins and MCP servers.
  env.CODEX_HOME = join(runDirectory, "codex-home");
  return {
    apiKey,
    env,
    config: {
      forced_login_method: "api",
      model_provider: "openai",
      model_instructions_file: join(runDirectory, "game-instructions.md"),
      features: { shell_tool: false, unified_exec: false },
      agents: { enabled: false },
    },
  };
}

export function threadOptions(runDirectory: string, model?: string): ThreadOptions {
  return {
    ...(model ? { model } : {}),
    workingDirectory: join(runDirectory, "context"),
    skipGitRepoCheck: true,
    sandboxMode: "read-only",
    approvalPolicy: "never",
    networkAccessEnabled: false,
    webSearchMode: "disabled",
  };
}

export async function createCodexBrain(apiKey: string, runDirectory: string, model?: string): Promise<Brain> {
  await mkdir(join(runDirectory, "codex-home"), { recursive: true });
  await mkdir(join(runDirectory, "context"), { recursive: true });
  await writeFile(join(runDirectory, "game-instructions.md"), GAME_INSTRUCTIONS, "utf8");
  const codex = new Codex(sdkOptions(apiKey, runDirectory, process.env));
  return {
    name: "Codex",
    model: model ?? "codex-default",
    pricing: null,
    async decide(prompt, signal) {
      const thread = codex.startThread(threadOptions(runDirectory, model));
      const turn = await thread.run(prompt, { outputSchema: decisionSchema, signal });
      const usage: Usage | null = turn.usage ? {
        input: turn.usage.input_tokens - turn.usage.cached_input_tokens,
        output: turn.usage.output_tokens,
        cacheRead: turn.usage.cached_input_tokens,
        cacheWrite: turn.usage.cache_write_input_tokens,
        thinking: turn.usage.reasoning_output_tokens,
      } : null;
      return { text: turn.finalResponse, usage };
    },
  };
}
