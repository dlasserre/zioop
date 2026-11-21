import { randomUUID } from "node:crypto";
import { mkdir } from "node:fs/promises";
import { join } from "node:path";
import { createCodexBrain } from "./brain.js";
import { createClaudeBrain } from "./claude-brain.js";
import { runAgent } from "./agent.js";
import { readConfig, redact } from "./config.js";
import { MODEL_ALIASES } from "./models.js";
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
    console.log(`Agents Zioop avec les SDK Codex et Claude

Codex : OPENAI_API_KEY ; Claude : ANTHROPIC_API_KEY.
Clés dans l'environnement ou .env.

npm start -- [--provider codex|claude] [--host 127.0.0.1] [--port 9090]
             [--model ${MODEL_ALIASES.join("|")}|ID] [--effort low|medium|high|xhigh|max]
             [--max-decisions 100] [--log-dir runs] [-v]

--model  Claude : alias ou identifiant complet (défaut opus). Codex : identifiant du modèle.
--effort Claude : profondeur de réflexion (défaut low, le moins coûteux).
--max-decisions nombre maximal d'appels à l'IA (chaque appel produit un plan de plusieurs actions).
-v       affiche le prompt envoyé, les réponses du serveur et les tokens par appel.

Raccourcis : npm run start:codex / npm run start:claude
Ctrl+C arrête le client. Une nouvelle connexion crée un nouvel agent.`);
  } else {
    if (!apiKey) throw new Error(`${keyVariable} manquante. Renseigne-la dans ton environnement ou dans clients/ai/.env (ignoré par Git).`);
    const runDirectory = join(config.logDirectory, `${config.provider}-${new Date().toISOString().replaceAll(":", "-")}-${randomUUID().slice(0, 8)}`);
    await mkdir(runDirectory, { recursive: true });
    const brain = config.claudeModel ? createClaudeBrain(apiKey, config.claudeModel, config.effort) : await createCodexBrain(apiKey, runDirectory, config.model);
    const journal = new Journal(join(runDirectory, "history.jsonl"));
    await connection.connect(config.host, config.port, stop.signal);
    connection.onDisconnect = () => stop.abort(new Error("Connexion au jeu fermée"));
    console.log(`Historique : ${journal.path}`);
    await runAgent({ connection, brain, journal, runDirectory, maxDecisions: config.maxDecisions, signal: stop.signal, verbose: config.verbose,
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
