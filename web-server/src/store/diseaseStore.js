// Persisted disease-analysis log (on-demand checks). Bounded ring, atomically
// written on each append + loaded at boot — same pattern as pumpLog. Best-effort
// writes never break an analysis.

const fs = require('fs');
const path = require('path');

const LOG_PATH =
  process.env.DISEASE_LOG_PATH || path.join(__dirname, '..', '..', 'data', 'disease-log.json');
const MAX_ENTRIES = Math.max(1, Number(process.env.DISEASE_LOG_MAX) || 200);

let seq = 0;
let entries = []; // oldest -> newest

function atomicWrite() {
  try {
    fs.mkdirSync(path.dirname(LOG_PATH), { recursive: true });
    const tmp = `${LOG_PATH}.tmp-${process.pid}`;
    fs.writeFileSync(tmp, JSON.stringify({ seq, entries }, null, 2));
    fs.renameSync(tmp, LOG_PATH);
  } catch {
    // Read-only fs / missing volume: keep the in-memory ring only.
  }
}

function load() {
  try {
    const parsed = JSON.parse(fs.readFileSync(LOG_PATH, 'utf8'));
    if (Array.isArray(parsed.entries)) {
      entries = parsed.entries.slice(-MAX_ENTRIES);
      seq = Number(parsed.seq) || entries.reduce((m, e) => Math.max(m, e.id || 0), 0);
    }
  } catch {
    entries = [];
    seq = 0;
  }
  return list();
}

// entry = { at, status, headline, label, confidence }
function append(entry) {
  seq += 1;
  const stored = { id: seq, ...entry };
  entries.push(stored);
  while (entries.length > MAX_ENTRIES) entries.shift();
  atomicWrite();
  return stored;
}

function list(limit) {
  const out = entries.slice().reverse(); // newest first
  return typeof limit === 'number' && limit > 0 ? out.slice(0, limit) : out;
}

module.exports = { load, append, list, LOG_PATH, MAX_ENTRIES };
