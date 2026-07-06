const express = require('express');
const { reflectState, upsertTelemetry } = require('../store/deviceStore');
const settingsStore = require('../store/settingsStore');

const router = express.Router();

// The store device_id the real pump is mirrored into, so the generic dashboard
// and the irrigation page can read live hardware state like any other device.
// Single pump in this system; matches the seed + irrigation page's PUMP_ID.
const PUMP_DEVICE_ID = 'main-pump';

// The pump hardware has NO onboard sensors. We still surface a node entry so the
// Irrigation page shows it reporting — but every reading is "n/a" rather than a
// fabricated number. Non-numeric values are ignored by the trend history/charts
// (see historySlice) and render verbatim in tables (see formatMetricValue).
const PUMP_NODE_ID = 'main-pump-node';
const PUMP_NODE_NA_METRICS = { pressure: 'n/a', flow_rate: 'n/a', temperature: 'n/a', voltage: 'n/a' };

// Thin CORS-clean relay between the browser/dashboard and a pump-zone node
// (ESP-WROOM-32 running pump-zone firmware, HTTP server at <target>/api/v1/relay
// accepting {"state":"on"|"off"} and returning {"relay_status":"ON"|"OFF"}).
//
// The browser can't POST JSON to the pump directly (CORS preflight the pump's
// esp_http_server doesn't answer), so all pump traffic goes through here. The
// pump TARGET and auto-off duration are server-owned config (settingsStore); the
// client posts only { state }. The pump holds NO state on this server beyond a
// single safety-timer per target.

// How long we wait on the pump before calling it unreachable.
const RELAY_TIMEOUT_MS = Number(process.env.PUMP_RELAY_TIMEOUT_MS) || 4000;
// Backend clamp for the auto-off duration (defense-in-depth; the store clamps too).
const AUTO_OFF_MIN = 1;
const AUTO_OFF_MAX = 60;

// One armed auto-off timer per pump target. Keyed by the normalized base URL.
// In-memory by design (matches the no-SD-wear ethos); lost on restart, after
// which the 5s GET poll re-establishes the pump's true state in the UI.
const timers = new Map(); // key -> { handle, autoOffAt }

// Normalize a client-supplied base URL: must be http(s), strip trailing slash.
function normalizeTarget(target) {
  if (typeof target !== 'string' || target.trim() === '') return null;
  let u;
  try {
    u = new URL(target.trim());
  } catch {
    return null;
  }
  if (u.protocol !== 'http:' && u.protocol !== 'https:') return null;
  return u.origin; // scheme://host[:port], no path/query/trailing slash
}

// The pump target + auto-off now live in server-owned settings (settingsStore),
// not the request. Read + normalize the configured pump URL.
function configuredTarget() {
  return normalizeTarget(settingsStore.get().pump.url);
}

function configuredAutoOffMinutes() {
  return clampMinutes(settingsStore.get().pump.autoOffMinutes);
}

function relayUrl(base) {
  return `${base}/api/v1/relay`;
}

function clampMinutes(raw) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return 5;
  return Math.min(AUTO_OFF_MAX, Math.max(AUTO_OFF_MIN, Math.round(n)));
}

// Perform one relay request to the pump with a hard timeout. Returns the parsed
// { relay_status } on success, or throws (caller renders it as online:false).
async function relay(base, method, bodyObj) {
  const controller = new AbortController();
  const t = setTimeout(() => controller.abort(), RELAY_TIMEOUT_MS);
  try {
    const res = await fetch(relayUrl(base), {
      method,
      signal: controller.signal,
      headers: bodyObj ? { 'Content-Type': 'application/json' } : undefined,
      body: bodyObj ? JSON.stringify(bodyObj) : undefined,
    });
    if (!res.ok) throw new Error(`pump responded ${res.status}`);
    return await res.json();
  } finally {
    clearTimeout(t);
  }
}

function clearTimer(key) {
  const existing = timers.get(key);
  if (existing) {
    clearTimeout(existing.handle);
    timers.delete(key);
  }
}

// Arm (or re-arm) the backend-authoritative auto-off for a pump. When it fires,
// the SERVER POSTs off to the pump on its own — independent of any browser — so
// a sleeping/closed tablet can't leave the pump running.
function armTimer(base, minutes) {
  clearTimer(base);
  const ms = minutes * 60 * 1000;
  const autoOffAt = new Date(Date.now() + ms).toISOString();
  const handle = setTimeout(async () => {
    timers.delete(base);
    try {
      const data = await relay(base, 'POST', { state: 'off' });
      mirror(data.relay_status);
      console.log(`[pump] auto-off fired for ${base}`);
    } catch (err) {
      // Pump unreachable at expiry: it may remain ON until reachable again.
      // (Only a firmware deadman would fully close this gap.)
      console.error(`[pump] auto-off FAILED for ${base}: ${err.message}`);
    }
  }, ms);
  timers.set(base, { handle, autoOffAt });
  return autoOffAt;
}

function armedAutoOffAt(key) {
  const t = timers.get(key);
  return t ? t.autoOffAt : null;
}

// Mirror the pump's real relay state into the store as a generic actuator, and
// surface the (sensorless) pump node as reporting-but-n/a so the Irrigation page
// shows it "sending data" without inventing fake sensor numbers.
function mirror(relayStatus) {
  reflectState({
    device_id: PUMP_DEVICE_ID,
    metrics: { running: relayStatus === 'ON' },
  });
  upsertTelemetry({ device_id: PUMP_NODE_ID, metrics: PUMP_NODE_NA_METRICS });
}

// GET /api/v1/pump/status  -> current state (polled). Target comes from settings.
router.get('/status', async (req, res) => {
  const base = configuredTarget();
  if (!base) {
    return res.status(400).json({ error: 'no valid pump.url configured in settings' });
  }
  try {
    const data = await relay(base, 'GET');
    mirror(data.relay_status);
    res.json({
      online: true,
      relay_status: data.relay_status,
      autoOffAt: armedAutoOffAt(base),
    });
  } catch (err) {
    res.json({ online: false, error: err.name === 'AbortError' ? 'timeout' : 'unreachable' });
  }
});

// POST /api/v1/pump/control  body: { state:"on"|"off" }
// Target + auto-off duration are read from server-owned settings.
router.post('/control', async (req, res) => {
  const { state } = req.body || {};
  const base = configuredTarget();
  if (!base) {
    return res.status(400).json({ error: 'no valid pump.url configured in settings' });
  }
  if (state !== 'on' && state !== 'off') {
    return res.status(400).json({ error: 'state must be "on" or "off"' });
  }

  try {
    const data = await relay(base, 'POST', { state });
    mirror(data.relay_status);
    // Arm the safety timer on ON; cancel it on manual OFF.
    let autoOffAt = null;
    if (state === 'on') {
      autoOffAt = armTimer(base, configuredAutoOffMinutes());
    } else {
      clearTimer(base);
    }
    res.json({ online: true, relay_status: data.relay_status, autoOffAt });
  } catch (err) {
    res.json({ online: false, error: err.name === 'AbortError' ? 'timeout' : 'unreachable' });
  }
});

module.exports = router;
