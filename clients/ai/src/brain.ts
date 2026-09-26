import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { Codex, type CodexOptions, type ThreadOptions } from "@openai/codex-sdk";
import { decisionSchema, GAME_INSTRUCTIONS } from "./commands.js";

export interface Brain {
  readonly name: string;
  readonly threadId: string | null;
  decide(prompt: string, signal: AbortSignal): Promise<string>;
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
  const thread = codex.startThread(threadOptions(runDirectory, model));
  return {
    name: "Codex",
    get threadId() { return thread.id; },
    async decide(prompt, signal) {
      const turn = await thread.run(prompt, { outputSchema: decisionSchema, signal });
      // Store no raw reasoning/tool transcripts in the application history.
      return turn.finalResponse;
    },
  };
}
