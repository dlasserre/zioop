import { appendFile, readFile } from "node:fs/promises";

export type HistoryEntry = {
  sequence: number;
  at: string;
  command: string;
  parameters: unknown;
  reason: string;
  response: { ok: boolean; data: unknown };
  durationMs: number;
};

export class Journal {
  private readonly entries: HistoryEntry[] = [];
  constructor(readonly path: string) {}

  async append(entry: Omit<HistoryEntry, "sequence" | "at">): Promise<HistoryEntry> {
    const complete = { ...entry, sequence: this.entries.length + 1, at: new Date().toISOString() };
    await appendFile(this.path, JSON.stringify(complete) + "\n", { encoding: "utf8", mode: 0o600 });
    this.entries.push(complete);
    return complete;
  }

  page(before: number | null = null, limit = 12): { entries: HistoryEntry[]; nextBefore: number | null } {
    if (!Number.isInteger(limit) || limit < 1 || limit > 20) throw new Error("History limit must be 1..20");
    const eligible = this.entries.filter((entry) => before === null || entry.sequence < before);
    const entries = eligible.slice(-limit);
    return { entries, nextBefore: eligible.length > entries.length ? entries[0]!.sequence : null };
  }
}

export async function readJournal(path: string): Promise<HistoryEntry[]> {
  const lines = (await readFile(path, "utf8")).split("\n");
  if (lines.at(-1) === "") lines.pop();
  return lines.map((line, index) => {
    try { return JSON.parse(line) as HistoryEntry; }
    catch { throw new Error(`Invalid JSONL history at line ${index + 1}`); }
  });
}
