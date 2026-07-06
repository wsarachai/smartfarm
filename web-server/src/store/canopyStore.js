// Persisted canopy-coverage history (for the AI Insights trend). Bounded ring,
// atomically written on a coarse cadence + loaded at boot — same pattern as
// waterStressStore. Best-effort writes never break the sampler.

const fs = require('fs');
const path = require('path');

const HISTORY_PATH =
  process.env.CANOPY_HISTORY_PATH ||
  path.join(__dirname, '..', '..', 'data', 'canopy-history.json');
const MAX_POINTS = Math.max(1, Number(process.env.CANOPY_HISTORY_MAX) || 2016); // ~7d @ 5min

let points = []; // oldest -> newest

function atomicWrite() {
  try {
    fs.mkdirSync(path.dirname(HISTORY_PATH), { recursive: true });
    const tmp = `${HISTORY_PATH}.tmp-${process.pid}`;
    fs.writeFileSync(tmp, JSON.stringify({ points }, null, 2));
    fs.renameSync(tmp, HISTORY_PATH);
  } catch {
    // Read-only fs / missing volume: keep the in-memory ring only.
  }
}

function load() {
  try {
    const parsed = JSON.parse(fs.readFileSync(HISTORY_PATH, 'utf8'));
    if (Array.isArray(parsed.points)) points = parsed.points.slice(-MAX_POINTS);
  } catch {
    points = [];
  }
  return list();
}

// point = { at, canopyPercent }
function append(point) {
  points.push(point);
  while (points.length > MAX_POINTS) points.shift();
  atomicWrite();
  return point;
}

function list(limit) {
  return typeof limit === 'number' && limit > 0 ? points.slice(-limit) : points.slice();
}

module.exports = { load, append, list, HISTORY_PATH, MAX_POINTS };
