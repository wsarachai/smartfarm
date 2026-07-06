// Water-stress estimator — ORCHESTRATOR side.
//
// The decision itself now lives in the smartfarm-ai container (a stateless HTTP
// service). This module keeps everything ELSE: it aggregates fresh sensor
// telemetry, calls the AI service with those inputs + the configured thresholds,
// smooths the returned band, caches the result, and appends a persisted history
// point on a coarse cadence. Reads (GET /api/v1/water-stress) serve the cache.
//
// Graceful degrade, no local fallback: if the AI service is unreachable the last
// known result is served marked `aiOnline:false` (or `unknown` if we never got
// one) — the decision logic is NOT duplicated here. Advisory only.

const settingsStore = require('./../store/settingsStore');
const { listDevices } = require('./../store/deviceStore');
const waterStressStore = require('./../store/waterStressStore');

const STALE_MS = Number(process.env.WATER_STRESS_STALE_MS) || 5 * 60 * 1000;
const SMOOTH_N = Math.max(1, Number(process.env.WATER_STRESS_SMOOTH_SAMPLES) || 5);
const LIVE_MS = Number(process.env.WATER_STRESS_LIVE_MS) || 30 * 1000;
const HISTORY_MS = Number(process.env.WATER_STRESS_HISTORY_MS) || 5 * 60 * 1000;
const AI_URL = process.env.AI_SERVICE_URL || 'http://smartfarm-ai:8000/water-stress';
const AI_TIMEOUT_MS = Number(process.env.AI_SERVICE_TIMEOUT_MS) || 4000;

const RISK_BY_BAND = { 1: 'low', 2: 'medium', 3: 'high' };

let ring = []; // recent numeric bands for smoothing
let lastResult = { risk: 'unknown', band: null, inputs: {}, factors: ['Starting up…'], at: null, aiOnline: null };
let lastHistoryAt = 0;
let timer = null;

function round1(n) {
  return n == null ? null : Math.round(n * 10) / 10;
}

// Average a metric across FRESH sensor nodes (device seen within STALE_MS).
function freshAvg(devices, key) {
  const now = Date.now();
  const vals = devices
    .filter((d) => d.type !== 'actuator')
    .filter((d) => d.lastSeen && now - Date.parse(d.lastSeen) <= STALE_MS)
    .map((d) => d.metrics?.[key])
    .filter((v) => typeof v === 'number' && Number.isFinite(v));
  if (vals.length === 0) return null;
  return vals.reduce((a, b) => a + b, 0) / vals.length;
}

function gatherInputs() {
  const devices = listDevices();
  return {
    soilMoisture: round1(freshAvg(devices, 'soil_moisture')),
    temperature: round1(freshAvg(devices, 'temperature')),
    humidity: round1(freshAvg(devices, 'humidity')),
  };
}

// POST inputs + thresholds to the AI decision service. Returns { band, risk,
// factors } or throws (timeout / unreachable / non-200).
async function decideRemote(inputs, thresholds) {
  const controller = new AbortController();
  const t = setTimeout(() => controller.abort(), AI_TIMEOUT_MS);
  try {
    const res = await fetch(AI_URL, {
      method: 'POST',
      signal: controller.signal,
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ inputs, thresholds }),
    });
    if (!res.ok) throw new Error(`ai responded ${res.status}`);
    return await res.json();
  } finally {
    clearTimeout(t);
  }
}

function maybeAppendHistory() {
  const now = Date.now();
  if (now - lastHistoryAt >= HISTORY_MS) {
    lastHistoryAt = now;
    const c = lastResult;
    waterStressStore.append({
      at: c.at,
      risk: c.risk,
      band: c.band,
      soil: c.inputs.soilMoisture ?? null,
      temp: c.inputs.temperature ?? null,
      humidity: c.inputs.humidity ?? null,
    });
  }
}

async function tick() {
  const cfg = settingsStore.get().waterStress;
  const inputs = gatherInputs();
  const at = new Date().toISOString();

  // No fresh soil reading — unknown regardless of the AI (don't bother calling).
  if (inputs.soilMoisture == null) {
    ring = [];
    lastResult = {
      risk: 'unknown',
      band: null,
      inputs,
      factors: ['No fresh soil-moisture reading — cannot estimate water stress.'],
      at,
      aiOnline: lastResult.aiOnline,
    };
    maybeAppendHistory();
    return;
  }

  try {
    const d = await decideRemote(inputs, cfg); // { band, risk, factors }
    if (d.band == null) {
      ring = [];
      lastResult = { risk: d.risk || 'unknown', band: null, inputs, factors: d.factors || [], at, aiOnline: true };
    } else {
      ring.push(d.band);
      while (ring.length > SMOOTH_N) ring.shift();
      const smoothedBand = Math.round(ring.reduce((a, b) => a + b, 0) / ring.length);
      lastResult = {
        risk: RISK_BY_BAND[smoothedBand],
        band: smoothedBand,
        inputs,
        factors: d.factors || [],
        at,
        aiOnline: true,
      };
    }
  } catch (err) {
    // AI unreachable: keep the last-known decision but flag offline; if we never
    // got one, it's unknown. Inputs are still refreshed so the card shows live data.
    const hadResult = lastResult.band != null;
    lastResult = {
      risk: hadResult ? lastResult.risk : 'unknown',
      band: hadResult ? lastResult.band : null,
      inputs,
      factors: hadResult ? lastResult.factors : ['AI service unreachable — no estimate yet.'],
      at,
      aiOnline: false,
    };
  }

  maybeAppendHistory();
}

function start() {
  if (timer) return;
  tick().catch((err) => console.error(`[water-stress] tick error: ${err.message}`));
  timer = setInterval(() => {
    tick().catch((err) => console.error(`[water-stress] tick error: ${err.message}`));
  }, LIVE_MS);
  if (timer.unref) timer.unref();
  console.log(`[water-stress] estimator started — decisions via ${AI_URL} every ${LIVE_MS}ms`);
}

function stop() {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
}

function current() {
  return { ...lastResult };
}

module.exports = { start, stop, current, tick, gatherInputs, _internals: { freshAvg, decideRemote } };
