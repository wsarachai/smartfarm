// Pump action log — an audit trail of every actuation + decision.
//
// Bounded in-memory ring (newest kept), atomically persisted to a small JSON
// file on the host volume on each append (same pattern as settings.json) and
// loaded at boot, so history survives restart. Pump actions are sparse events
// (a handful a day), so per-event writes are negligible SD wear — unlike the
// per-sample telemetry the RAM-only stores guard against. Logging must never
// break actuation, so every write is best-effort (swallows fs errors).

const fs = require('fs');
const path = require('path');

const LOG_PATH =
  process.env.PUMP_LOG_PATH || path.join(__dirname, '..', '..', 'data', 'pump-log.json');
const MAX_ENTRIES = Math.max(1, Number(process.env.PUMP_LOG_MAX) || 500);

let seq = 0;
let entries = []; // oldest -> newest, length <= MAX_ENTRIES

function atomicWrite() {
  try {
    fs.mkdirSync(path.dirname(LOG_PATH), { recursive: true });
    const tmp = `${LOG_PATH}.tmp-${process.pid}`;
    fs.writeFileSync(tmp, JSON.stringify({ seq, entries }, null, 2));
    fs.renameSync(tmp, LOG_PATH);
  } catch {
    // Read-only fs / missing volume: keep running from the in-memory ring.
  }
}

// Load persisted log at boot. A missing/unreadable/corrupt file starts empty.
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

// Append one event. `evt` = { action:"on"|"off"|"skip", source, ok, label?,
// durationMinutes?, moisture?, error?, note? }. Returns the stored entry.
function append(evt) {
  seq += 1;
  const entry = { id: seq, at: new Date().toISOString(), ...evt };
  entries.push(entry);
  while (entries.length > MAX_ENTRIES) entries.shift();
  atomicWrite();
  return entry;
}

// Newest-first, optionally limited.
function list(limit) {
  const out = entries.slice().reverse();
  return typeof limit === 'number' && limit > 0 ? out.slice(0, limit) : out;
}

module.exports = { load, append, list, LOG_PATH, MAX_ENTRIES };
