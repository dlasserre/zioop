import { randomUUID } from "node:crypto";
import { mkdir } from "node:fs/promises";
import { join } from "node:path";
import { createCodexBrain } from "./brain.js";
import { createClaudeBrain } from "./claude-brain.js";
import { runAgent } from "./agent.js";
import { readConfig, redact } from "./config.js";
import { Journal } from "./history.js";
import { GameConnection } from "./protocol.js";

let apiKey = "";
const stop = new AbortController();
const connection = new GameConnection();
let userInterrupted = false;
const interrupt = () => { userInterrupted = true; stop.abort(new Error("Arrêt demandé")); connection.close(); };
process.once("SIGINT", interrupt);
process.once("SIGTERM", interrupt);

try {
  const config = readConfig(process.argv.slice(2), process.env);
  const keyVariable = config.provider === "codex" ? "OPENAI_API_KEY" : "ANTHROPIC_API_KEY";
  apiKey = (config.provider === "codex" ? (process.env.OPENAI_API_KEY?.trim() || process.env.CODEX_API_KEY) : process.env.ANTHROPIC_API_KEY)?.trim() ?? "";
  if (config.help) {
    console.log(`Agents Zioop avec les SDK Codex et Claude\n\nCodex : OPENAI_API_KEY ; Claude : ANTHROPIC_API_KEY.\nClés dans l'environnement ou .env.\n\nnpm start -- [--provider codex|claude] [--host 127.0.0.1] [--port 9090]\n             [--model NOM] [--max-decisions 100] [--log-dir runs]\n\nRaccourcis : npm run start:codex / npm run start:claude\nCtrl+C arrête le client. HISTORY consulte le journal de la partie.\nUne nouvelle connexion crée un nouvel agent et une nouvelle conversation.`);
  } else {
    if (!apiKey) throw new Error(`${keyVariable} manquante. Renseigne-la dans ton environnement ou dans clients/ai/.env (ignoré par Git).`);
    const runDirectory = join(config.logDirectory, `${config.provider}-${new Date().toISOString().replaceAll(":", "-")}-${randomUUID().slice(0, 8)}`);
    await mkdir(runDirectory, { recursive: true });
    const brain = await (config.provider === "codex" ? createCodexBrain : createClaudeBrain)(apiKey, runDirectory, config.model);
    const journal = new Journal(join(runDirectory, "history.jsonl"));
    await connection.connect(config.host, config.port, stop.signal);
    connection.onDisconnect = () => stop.abort(new Error("Connexion au jeu fermée"));
    console.log(`Historique : ${journal.path}`);
    await runAgent({ connection, brain, journal, runDirectory, maxDecisions: config.maxDecisions, signal: stop.signal,
      log: (line) => console.log(redact(line, apiKey)) });
  }
} catch (error) {
  console.error(redact(error instanceof Error ? error.message : String(error), apiKey));
  process.exitCode = userInterrupted ? 130 : 1;
} finally {
  connection.onDisconnect = undefined;
  connection.close();
  process.off("SIGINT", interrupt);
  process.off("SIGTERM", interrupt);
}
