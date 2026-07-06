// Disease detection — ORCHESTRATOR side (feature 3). On-demand only: an analysis
// runs when the UI triggers it (POST /api/v1/disease/analyze), not on a timer
// (inference is heavy). Grabs the latest frame, POSTs it to the AI /disease
// classifier, applies the confidence threshold + a healthy/disease headline,
// caches the result, and appends a persisted log entry.
//
// Graceful/honest states: no fresh frame, AI offline, model not loaded, and
// inconclusive (top-1 below threshold) each surface as their own headline.

const settingsStore = require('./../store/settingsStore');
const frameStore = require('./../store/frameStore');
const diseaseStore = require('./../store/diseaseStore');

const AI_URL = process.env.DISEASE_SERVICE_URL || 'http://smartfarm-ai:8000/disease';
const AI_TIMEOUT_MS = Number(process.env.DISEASE_TIMEOUT_MS) || 30000; // inference is slow on the Nano
const FRAME_STALE_MS = Number(process.env.DISEASE_FRAME_STALE_MS) || 10 * 60 * 1000;

let lastResult = { status: 'idle', headline: 'Not analyzed yet', top: [], at: null, aiOnline: null };
let inFlight = null; // de-dupe concurrent analyze() calls

// "Tomato___Late_blight" -> "Tomato — Late blight"
function prettyLabel(label) {
  return String(label).replace(/___/g, ' — ').replace(/_/g, ' ').trim();
}

function isHealthy(label) {
  return /healthy/i.test(String(label));
}

async function classifyRemote(buf) {
  const controller = new AbortController();
  const t = setTimeout(() => controller.abort(), AI_TIMEOUT_MS);
  try {
    const res = await fetch(AI_URL, {
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

async function runAnalysis() {
  const at = new Date().toISOString();
  const frame = frameStore.getFrame();

  if (!frame || Date.now() - frame.receivedAt > FRAME_STALE_MS) {
    lastResult = {
      status: 'no_frame',
      headline: frame ? 'Latest camera frame is stale' : 'No camera frame to analyze',
      top: [],
      at,
      aiOnline: lastResult.aiOnline,
    };
    return lastResult;
  }

  let data;
  try {
    data = await classifyRemote(frame.buf);
  } catch {
    lastResult = { status: 'ai_offline', headline: 'AI service unreachable', top: [], at, aiOnline: false };
    return lastResult;
  }

  if (!data.modelLoaded) {
    lastResult = {
      status: 'model_not_loaded',
      headline: 'Disease model not loaded on smartfarm-ai',
      detail: data.error || null,
      top: [],
      at,
      aiOnline: true,
    };
    return lastResult;
  }

  const threshold = settingsStore.get().disease.confidenceThreshold;
  const top = (data.topK || []).map((t) => ({ label: prettyLabel(t.label), raw: t.label, confidence: t.confidence }));
  const best = top[0];

  let status;
  let headline;
  if (!best) {
    status = 'inconclusive';
    headline = 'No prediction';
  } else if (best.confidence < threshold) {
    status = 'inconclusive';
    headline = `Inconclusive — low confidence (top: ${best.label} ${best.confidence}%)`;
  } else if (isHealthy(best.raw)) {
    status = 'healthy';
    headline = `Healthy (${best.confidence}%)`;
  } else {
    status = 'disease';
    headline = `Possible: ${best.label} (${best.confidence}%)`;
  }

  lastResult = { status, headline, top, at, aiOnline: true };
  diseaseStore.append({
    at,
    status,
    headline,
    label: best ? best.raw : null,
    confidence: best ? best.confidence : null,
  });
  return lastResult;
}

// Trigger an analysis; de-dupes concurrent calls (heavy inference).
function analyze() {
  if (!inFlight) {
    inFlight = runAnalysis().finally(() => {
      inFlight = null;
    });
  }
  return inFlight;
}

function current() {
  return { ...lastResult, analyzing: inFlight !== null };
}

module.exports = { analyze, current };
