// Server-owned dashboard settings (global, single source of truth).
//
// These are the config blocks the Settings page used to keep in browser
// localStorage (per-browser): the CAMERA SOURCE prefs (where the browser reads
// live/snapshot frames) and the PUMP CONTROL config (target URL, label, auto-off
// duration). They are now global: every client loads them from here on open, and
// a save updates them for everyone. Persisted to a small JSON file on the host
// volume, written atomically (temp + rename) so a reader never sees a torn file.
//
// This mirrors cameraConfig.js (the camera DEVICE config the firmware pulls) but
// is a distinct concern and a distinct file: camera-config.json is a firmware
// contract; settings.json is dashboard/server config. Changes are rare (only a
// dashboard edit), so this is not the per-sample SD hammering we guard against.

const fs = require('fs');
const path = require('path');

const SETTINGS_PATH =
  process.env.SETTINGS_PATH ||
  path.join(__dirname, '..', '..', 'data', 'settings.json');

const AUTO_OFF_MIN = 1;
const AUTO_OFF_MAX = 60;
const SOURCE_MODES = ['relay', 'custom'];

function defaults() {
  return {
    cameraSource: {
      sourceMode: SOURCE_MODES.includes(process.env.CAMERA_SOURCE_MODE)
        ? process.env.CAMERA_SOURCE_MODE
        : 'relay',
      streamUrl: process.env.CAMERA_STREAM_URL || 'http://192.168.0.3:81/stream',
      snapshotUrl: process.env.CAMERA_SNAPSHOT_URL || '/api/v1/camera/frame.jpg',
    },
    pump: {
      url: process.env.PUMP_URL || 'http://192.168.0.5',
      label: process.env.PUMP_LABEL || 'Main Pump',
      autoOffMinutes: clampMinutes(process.env.PUMP_AUTO_OFF_MINUTES, 5),
    },
  };
}

function clampMinutes(raw, fallback) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  return Math.min(AUTO_OFF_MAX, Math.max(AUTO_OFF_MIN, Math.round(n)));
}

function nonEmptyString(raw, fallback) {
  if (typeof raw !== 'string') return fallback;
  const trimmed = raw.trim();
  return trimmed || fallback;
}

function isHttpUrl(raw) {
  if (typeof raw !== 'string' || raw.trim() === '') return false;
  try {
    const u = new URL(raw.trim());
    return u.protocol === 'http:' || u.protocol === 'https:';
  } catch {
    return false;
  }
}

let settings = defaults();

// Validate a partial patch { cameraSource?, pump? } against the current settings.
// Deep-merges by section. Returns { ok, value } or { ok:false, error }.
// Server-authoritative: pump.url must be http(s) (the server fetches it),
// autoOffMinutes clamps 1..60, label non-empty; sourceMode is a relay|custom
// enum; camera stream/snapshot URLs are just non-empty strings (browser hints).
function validate(patch) {
  const next = {
    cameraSource: { ...settings.cameraSource },
    pump: { ...settings.pump },
  };
  const p = patch || {};

  if ('cameraSource' in p) {
    const cs = p.cameraSource || {};
    if ('sourceMode' in cs) {
      if (!SOURCE_MODES.includes(cs.sourceMode))
        return { ok: false, error: `sourceMode must be one of ${SOURCE_MODES.join(', ')}` };
      next.cameraSource.sourceMode = cs.sourceMode;
    }
    if ('streamUrl' in cs) {
      const v = nonEmptyString(cs.streamUrl, null);
      if (v === null) return { ok: false, error: 'streamUrl must be a non-empty string' };
      next.cameraSource.streamUrl = v;
    }
    if ('snapshotUrl' in cs) {
      const v = nonEmptyString(cs.snapshotUrl, null);
      if (v === null) return { ok: false, error: 'snapshotUrl must be a non-empty string' };
      next.cameraSource.snapshotUrl = v;
    }
  }

  if ('pump' in p) {
    const pump = p.pump || {};
    if ('url' in pump) {
      if (!isHttpUrl(pump.url))
        return { ok: false, error: 'pump.url must be a valid http(s) URL' };
      next.pump.url = pump.url.trim();
    }
    if ('label' in pump) {
      const v = nonEmptyString(pump.label, null);
      if (v === null) return { ok: false, error: 'pump.label must be a non-empty string' };
      next.pump.label = v;
    }
    if ('autoOffMinutes' in pump) {
      const n = Number(pump.autoOffMinutes);
      if (!Number.isFinite(n) || n < AUTO_OFF_MIN || n > AUTO_OFF_MAX)
        return { ok: false, error: `autoOffMinutes must be ${AUTO_OFF_MIN}..${AUTO_OFF_MAX}` };
      next.pump.autoOffMinutes = Math.round(n);
    }
  }

  return { ok: true, value: next };
}

function atomicWrite(obj) {
  fs.mkdirSync(path.dirname(SETTINGS_PATH), { recursive: true });
  const tmp = `${SETTINGS_PATH}.tmp-${process.pid}`;
  fs.writeFileSync(tmp, JSON.stringify(obj, null, 2));
  fs.renameSync(tmp, SETTINGS_PATH); // atomic on the same filesystem
}

// Load persisted settings at boot; seed + persist defaults if absent/unreadable.
// A persisted file is re-validated (drops unknown/invalid fields back to default)
// so a hand-edited or stale file can't wedge the server.
function load() {
  try {
    const parsed = JSON.parse(fs.readFileSync(SETTINGS_PATH, 'utf8'));
    const res = validate(parsed);
    settings = res.ok ? res.value : defaults();
  } catch {
    settings = defaults();
    try {
      atomicWrite(settings);
    } catch {
      // Read-only fs / missing volume: run from in-memory defaults.
    }
  }
  return get();
}

function get() {
  return {
    cameraSource: { ...settings.cameraSource },
    pump: { ...settings.pump },
  };
}

function update(patch) {
  const res = validate(patch);
  if (!res.ok) return res;
  settings = res.value;
  atomicWrite(settings);
  return { ok: true, value: get() };
}

module.exports = { load, get, update, SETTINGS_PATH, AUTO_OFF_MIN, AUTO_OFF_MAX };
