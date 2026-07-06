// Canopy-coverage estimator — ORCHESTRATOR side (feature 2).
//
// The decision (green-pixel %) lives in the smartfarm-ai container. This module
// keeps everything else: on a tick it grabs the latest camera frame from the
// RAM frameStore, POSTs the JPEG + HSV thresholds to the AI /canopy endpoint,
// smooths the returned %, caches it (+ the mask-preview PNG in RAM), and appends
// a persisted history point on a coarse cadence. Reads serve the cache.
//
// Graceful degrade, no local fallback: no fresh frame -> "unknown"; AI
// unreachable -> last-known marked aiOnline:false. Advisory only.

const settingsStore = require('./../store/settingsStore');
const frameStore = require('./../store/frameStore');
const canopyStore = require('./../store/canopyStore');

const SMOOTH_N = Math.max(1, Number(process.env.CANOPY_SMOOTH_SAMPLES) || 5);
const LIVE_MS = Number(process.env.CANOPY_LIVE_MS) || 60 * 1000;
const HISTORY_MS = Number(process.env.CANOPY_HISTORY_MS) || 5 * 60 * 1000;
const FRAME_STALE_MS = Number(process.env.CANOPY_FRAME_STALE_MS) || 10 * 60 * 1000;
const AI_URL = process.env.CANOPY_SERVICE_URL || 'http://smartfarm-ai:8000/canopy';
const AI_TIMEOUT_MS = Number(process.env.AI_SERVICE_TIMEOUT_MS) || 8000;

let ring = []; // recent percents for smoothing
let lastResult = { canopyPercent: null, factors: ['Starting up…'], at: null, aiOnline: null, width: null, height: null };
let lastMaskPng = null; // base64 PNG (RAM only — a live debug preview)
let lastHistoryAt = 0;
let timer = null;

function query(cfg) {
  const p = new URLSearchParams({
    hueMinDeg: cfg.hueMinDeg,
    hueMaxDeg: cfg.hueMaxDeg,
    satMinPct: cfg.satMinPct,
    valMinPct: cfg.valMinPct,
  });
  return `${AI_URL}?${p.toString()}`;
}

// POST the JPEG (+ thresholds via query) to the AI. Returns the parsed result or
// throws (timeout / unreachable / non-200).
async function analyzeRemote(buf, cfg) {
  const controller = new AbortController();
  const t = setTimeout(() => controller.abort(), AI_TIMEOUT_MS);
  try {
    const res = await fetch(query(cfg), {
      method: 'POST',
      signal: controller.signal,
      headers: { 'Content-Type': 'image/jpeg' },
      body: buf,
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
    canopyStore.append({ at: lastResult.at, canopyPercent: lastResult.canopyPercent });
  }
}

async function tick() {
  const cfg = settingsStore.get().canopy;
  const frame = frameStore.getFrame();
  const at = new Date().toISOString();

  // No fresh frame — unknown regardless of the AI (don't bother calling).
  if (!frame || Date.now() - frame.receivedAt > FRAME_STALE_MS) {
    ring = [];
    lastMaskPng = null;
    lastResult = {
      canopyPercent: null,
      factors: [frame ? 'Latest camera frame is stale — cannot estimate canopy.' : 'No camera frame yet — cannot estimate canopy.'],
      at,
      aiOnline: lastResult.aiOnline,
      width: null,
      height: null,
    };
    maybeAppendHistory();
    return;
  }

  try {
    const d = await analyzeRemote(frame.buf, cfg); // { canopyPercent, factors, maskPng, width, height }
    const pct = Number(d.canopyPercent);
    ring.push(pct);
    while (ring.length > SMOOTH_N) ring.shift();
    const smoothed = Math.round((ring.reduce((a, b) => a + b, 0) / ring.length) * 10) / 10;
    lastMaskPng = typeof d.maskPng === 'string' ? d.maskPng : null;
    lastResult = {
      canopyPercent: smoothed,
      factors: d.factors || [],
      at,
      aiOnline: true,
      width: d.width ?? null,
      height: d.height ?? null,
    };
  } catch (err) {
    const had = lastResult.canopyPercent != null;
    lastResult = {
      canopyPercent: had ? lastResult.canopyPercent : null,
      factors: had ? lastResult.factors : ['AI service unreachable — no estimate yet.'],
      at,
      aiOnline: false,
      width: lastResult.width,
      height: lastResult.height,
    };
  }

  maybeAppendHistory();
}

function start() {
  if (timer) return;
  tick().catch((err) => console.error(`[canopy] tick error: ${err.message}`));
  timer = setInterval(() => {
    tick().catch((err) => console.error(`[canopy] tick error: ${err.message}`));
  }, LIVE_MS);
  if (timer.unref) timer.unref();
  console.log(`[canopy] estimator started — decisions via ${AI_URL} every ${LIVE_MS}ms`);
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

// Latest mask-preview PNG as a Buffer, or null. RAM-only (not persisted).
function previewPng() {
  return lastMaskPng ? Buffer.from(lastMaskPng, 'base64') : null;
}

module.exports = { start, stop, current, previewPng, tick };
