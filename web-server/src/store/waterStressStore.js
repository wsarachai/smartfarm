// Persisted water-stress risk history (for the AI Insights trend).
//
// A bounded in-memory ring atomically written to a JSON file on the host volume
// and loaded at boot — same pattern as pumpLog. The engine appends one point on
// a COARSE cadence (~5 min, not the 60s live tick), so writes stay modest
// (~288/day) despite persisting. Best-effort writes never break the sampler.

const fs = require('fs');
const path = require('path');

const HISTORY_PATH =
  process.env.WATER_STRESS_HISTORY_PATH ||
  path.join(__dirname, '..', '..', 'data', 'water-stress-history.json');
// ~7 days at one point / 5 min.
const MAX_POINTS = Math.max(1, Number(process.env.WATER_STRESS_HISTORY_MAX) || 2016);

let points = []; // oldest -> newest, length <= MAX_POINTS

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

// point = { at, risk, band, soil, temp, humidity }
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
