// Shared pump command layer used by BOTH the /api/v1/pump route and the
// irrigation scheduler. Owns the CORS-clean relay to a pump-zone node, the
// server-authoritative auto-off safety timer (one per target), and mirroring
// the pump's real state into the device store. The pump target + auto-off
// duration come from server-owned settings (settingsStore); callers pass only
// intent ("on"/"off", optionally an explicit run duration for scheduled runs).

const { reflectState, upsertTelemetry } = require('./deviceStore');
const settingsStore = require('./settingsStore');
const pumpLog = require('./pumpLog');

// The store device_id the real pump is mirrored into, so the generic dashboard
// and the irrigation page can read live hardware state like any other device.
const PUMP_DEVICE_ID = 'main-pump';
// Sensorless pump node: surfaced as reporting-but-n/a (never fabricated numbers).
const PUMP_NODE_ID = 'main-pump-node';
const PUMP_NODE_NA_METRICS = { pressure: 'n/a', flow_rate: 'n/a', temperature: 'n/a', voltage: 'n/a' };

const RELAY_TIMEOUT_MS = Number(process.env.PUMP_RELAY_TIMEOUT_MS) || 4000;
const AUTO_OFF_MIN = 1;
const AUTO_OFF_MAX = 60;

// One armed auto-off timer per pump target, keyed by normalized base URL.
// In-memory by design (matches the no-SD-wear ethos); lost on restart, after
// which the poll re-establishes the pump's true state in the UI.
const timers = new Map(); // key -> { handle, autoOffAt }

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

// One relay request with a hard timeout. Returns parsed { relay_status } or throws.
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

// Arm (or re-arm) the backend-authoritative auto-off. When it fires, the SERVER
// POSTs off on its own — independent of any browser or the scheduler. `meta.label`
// (a scheduled run's name) is carried through so the auto-off is logged with the
// run it ended; absent => a manual safety-window auto-off.
function armTimer(base, minutes, meta = {}) {
  clearTimer(base);
  const ms = minutes * 60 * 1000;
  const autoOffAt = new Date(Date.now() + ms).toISOString();
  const handle = setTimeout(async () => {
    timers.delete(base);
    try {
      const data = await relay(base, 'POST', { state: 'off' });
      mirror(data.relay_status);
      pumpLog.append({ action: 'off', source: 'auto-off', ok: true, label: meta.label || null });
      console.log(`[pump] auto-off fired for ${base}`);
    } catch (err) {
      pumpLog.append({ action: 'off', source: 'auto-off', ok: false, label: meta.label || null, error: err.message });
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
// surface the sensorless pump node as reporting-but-n/a.
function mirror(relayStatus) {
  reflectState({ device_id: PUMP_DEVICE_ID, metrics: { running: relayStatus === 'ON' } });
  upsertTelemetry({ device_id: PUMP_NODE_ID, metrics: PUMP_NODE_NA_METRICS });
}

// --- High-level API (used by the route + scheduler) -----------------------

// Read the configured pump's current state. Resolves to
// { online, relay_status, autoOffAt } or { online:false, error }.
async function getStatus() {
  const base = configuredTarget();
  if (!base) return { online: false, error: 'no valid pump.url configured in settings' };
  try {
    const data = await relay(base, 'GET');
    mirror(data.relay_status);
    return { online: true, relay_status: data.relay_status, autoOffAt: armedAutoOffAt(base) };
  } catch (err) {
    return { online: false, error: err.name === 'AbortError' ? 'timeout' : 'unreachable' };
  }
}

// Command the pump on/off. On "on", arms the auto-off with `runMinutes` if given
// (a scheduled run's duration), else the configured safety window. On "off",
// clears the timer. Every attempt is logged (pumpLog) with its `source`
// ("manual" | "schedule") and outcome. Resolves like getStatus().
async function command(state, { runMinutes, source = 'manual', label = null } = {}) {
  const base = configuredTarget();
  if (!base) return { online: false, error: 'no valid pump.url configured in settings' };
  if (state !== 'on' && state !== 'off') return { online: false, error: 'state must be "on" or "off"' };
  try {
    const data = await relay(base, 'POST', { state });
    mirror(data.relay_status);
    let autoOffAt = null;
    let durationMinutes = null;
    if (state === 'on') {
      durationMinutes = runMinutes != null ? clampMinutes(runMinutes) : configuredAutoOffMinutes();
      autoOffAt = armTimer(base, durationMinutes, { label });
    } else {
      clearTimer(base);
    }
    pumpLog.append({ action: state, source, ok: true, label, durationMinutes });
    return { online: true, relay_status: data.relay_status, autoOffAt };
  } catch (err) {
    const error = err.name === 'AbortError' ? 'timeout' : 'unreachable';
    pumpLog.append({ action: state, source, ok: false, label, error });
    return { online: false, error };
  }
}

module.exports = {
  getStatus,
  command,
  configuredTarget,
  PUMP_DEVICE_ID,
  AUTO_OFF_MIN,
  AUTO_OFF_MAX,
};
