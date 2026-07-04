const express = require('express');
const { reflectState } = require('../store/deviceStore');

const router = express.Router();

// The store device_id the real pump is mirrored into, so the generic dashboard
// and the irrigation page can read live hardware state like any other device.
// Single pump in this system; matches the seed + irrigation page's PUMP_ID.
const PUMP_DEVICE_ID = 'main-pump';

// Thin CORS-clean relay between the browser/dashboard and a pump-zone node
// (ESP-WROOM-32 running pump-zone firmware, HTTP server at <target>/api/v1/relay
// accepting {"state":"on"|"off"} and returning {"relay_status":"ON"|"OFF"}).
//
// The browser can't POST JSON to the pump directly (CORS preflight the pump's
// esp_http_server doesn't answer), so all pump traffic goes through here. The
// pump holds NO state on this server beyond a single safety-timer per target.

// How long we wait on the pump before calling it unreachable.
const RELAY_TIMEOUT_MS = Number(process.env.PUMP_RELAY_TIMEOUT_MS) || 4000;
// Backend clamp for the auto-off duration (defense-in-depth; the UI clamps too).
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

// Mirror the pump's real relay state into the store as a generic actuator.
function mirror(relayStatus) {
  reflectState({
    device_id: PUMP_DEVICE_ID,
    metrics: { running: relayStatus === 'ON' },
  });
}

// GET /api/v1/pump/status?target=http://192.168.0.4  -> current state (polled)
router.get('/status', async (req, res) => {
  const base = normalizeTarget(req.query.target);
  if (!base) {
    return res.status(400).json({ error: 'valid http(s) target query param required' });
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

// POST /api/v1/pump/control  body: { target, state:"on"|"off", autoOffMinutes }
router.post('/control', async (req, res) => {
  const { target, state, autoOffMinutes } = req.body || {};
  const base = normalizeTarget(target);
  if (!base) {
    return res.status(400).json({ error: 'valid http(s) target required' });
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
      autoOffAt = armTimer(base, clampMinutes(autoOffMinutes));
    } else {
      clearTimer(base);
    }
    res.json({ online: true, relay_status: data.relay_status, autoOffAt });
  } catch (err) {
    res.json({ online: false, error: err.name === 'AbortError' ? 'timeout' : 'unreachable' });
  }
});

module.exports = router;
