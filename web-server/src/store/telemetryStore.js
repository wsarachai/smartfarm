// Persisted telemetry history store for Live Operations trend charts.
// Stores snapshots of all device telemetry metrics with timestamp.
// Retains data up to 7 days (168 hours) and automatically prunes out-of-range points.

const fs = require('fs');
const path = require('path');
const { listDevices, setTelemetryListener } = require('./deviceStore');

const HISTORY_PATH =
  process.env.TELEMETRY_HISTORY_PATH ||
  path.join(__dirname, '..', '..', 'data', 'telemetry-history.json');

// 7 Days Retention Window (168 hours)
const RETENTION_MS = Number(process.env.TELEMETRY_RETENTION_MS) || 7 * 24 * 60 * 60 * 1000;
const SNAPSHOT_CADENCE_MS = 15 * 1000; // 15 seconds

let points = []; // oldest -> newest: { t: timestamp_ms, values: { "deviceId::metric": number } }
let lastSnapshotAt = 0;

function pruneOldPoints() {
  const cutoff = Date.now() - RETENTION_MS;
  points = points.filter((p) => typeof p.t === 'number' && p.t >= cutoff);
}

function atomicWrite() {
  try {
    fs.mkdirSync(path.dirname(HISTORY_PATH), { recursive: true });
    const tmp = `${HISTORY_PATH}.tmp-${process.pid}`;
    fs.writeFileSync(tmp, JSON.stringify({ points }, null, 2));
    fs.renameSync(tmp, HISTORY_PATH);
  } catch {
    // Read-only filesystem fallback: retain in-memory points
  }
}

function load() {
  try {
    const parsed = JSON.parse(fs.readFileSync(HISTORY_PATH, 'utf8'));
    if (Array.isArray(parsed.points)) {
      points = parsed.points;
      pruneOldPoints();
    }
  } catch {
    points = [];
  }
  return points.slice();
}

// Collect a snapshot from deviceStore
function captureSnapshot() {
  const now = Date.now();
  if (now - lastSnapshotAt < SNAPSHOT_CADENCE_MS) return;
  lastSnapshotAt = now;

  const devices = listDevices();
  const values = {};
  for (const device of devices) {
    for (const [key, value] of Object.entries(device.metrics || {})) {
      if (typeof value === 'number' && !Number.isNaN(value)) {
        values[`${device.device_id}::${key}`] = value;
      }
    }
  }

  if (Object.keys(values).length === 0) return;

  points.push({ t: now, values });
  pruneOldPoints();
  atomicWrite();
}

// Downsample array of points into fixed time buckets
function downsample(rawPoints, bucketMs) {
  if (!rawPoints || rawPoints.length === 0) return [];
  const buckets = new Map();

  for (const point of rawPoints) {
    const bucketKey = Math.floor(point.t / bucketMs) * bucketMs;
    if (!buckets.has(bucketKey)) {
      buckets.set(bucketKey, []);
    }
    buckets.get(bucketKey).push(point);
  }

  const result = [];
  for (const [bucketTime, bucketPoints] of buckets.entries()) {
    const aggValues = {};
    const keys = new Set();
    bucketPoints.forEach((p) => Object.keys(p.values || {}).forEach((k) => keys.add(k)));

    keys.forEach((key) => {
      const nums = bucketPoints.map((p) => p.values[key]).filter((v) => typeof v === 'number');
      if (nums.length > 0) {
        const avg = nums.reduce((sum, v) => sum + v, 0) / nums.length;
        aggValues[key] = Number(avg.toFixed(2));
      }
    });

    result.push({ t: bucketTime, values: aggValues });
  }

  return result.sort((a, b) => a.t - b.t);
}

function getHistory(range = 'current') {
  const now = Date.now();
  const normalizedRange = range.toLowerCase();

  if (normalizedRange === 'hour') {
    const cutoff = now - 60 * 60 * 1000;
    const filtered = points.filter((p) => p.t >= cutoff);
    return downsample(filtered, 60 * 1000); // 1-minute resolution (~60 points)
  } else if (normalizedRange === 'day') {
    const cutoff = now - 24 * 60 * 60 * 1000;
    const filtered = points.filter((p) => p.t >= cutoff);
    return downsample(filtered, 15 * 60 * 1000); // 15-minute resolution (~96 points)
  } else if (normalizedRange === 'week' || normalizedRange === '7days' || normalizedRange === '7d') {
    const cutoff = now - 7 * 24 * 60 * 60 * 1000;
    const filtered = points.filter((p) => p.t >= cutoff);
    return downsample(filtered, 60 * 60 * 1000); // 1-hour resolution (~168 points)
  }
  // 'current' - last 10 minutes raw
  const cutoff = now - 10 * 60 * 1000;
  return points.filter((p) => p.t >= cutoff);
}

// Load persisted history at startup & prune out-of-range points
load();

// Register telemetry updates listener & interval capture
setTelemetryListener(captureSnapshot);
setInterval(captureSnapshot, 15000);

// Hourly cleanup job to discard out-of-range data even when idle
setInterval(() => {
  pruneOldPoints();
  atomicWrite();
}, 60 * 60 * 1000);

module.exports = { load, captureSnapshot, getHistory, pruneOldPoints, HISTORY_PATH, RETENTION_MS };
