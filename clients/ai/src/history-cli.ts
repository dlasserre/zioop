import { parseArgs } from "node:util";
import { readJournal } from "./history.js";

try {
  const { values } = parseArgs({ options: { file: { type: "string" }, last: { type: "string", default: "20" } } });
  if (!values.file) throw new Error("Usage: npm run history -- --file runs/<session>/history.jsonl [--last 20]");
  const last = Number(values.last);
  if (!Number.isSafeInteger(last) || last < 1) throw new Error("--last must be a positive integer");
  const entries = await readJournal(values.file);
  for (const entry of entries.slice(-last)) console.log(JSON.stringify(entry, null, 2));
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
}
