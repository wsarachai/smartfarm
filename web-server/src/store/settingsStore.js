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
// Scheduled-run duration shares the safety auto-off bound (the run ends via the
// same auto-off timer), so an entry can't outlast the safety window.
const DURATION_MIN = AUTO_OFF_MIN;
const DURATION_MAX = AUTO_OFF_MAX;
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
    // Auto-mode irrigation: a server-run schedule + moisture guard. `auto` is the
    // global mode flag (scheduler only acts when true). Empty entries by default.
    irrigation: {
      auto: false,
      timezone: process.env.IRRIGATION_TZ || 'Asia/Bangkok',
      moistureThreshold: clampPercent(process.env.IRRIGATION_MOISTURE_THRESHOLD, 60),
      entries: [],
    },
    // Water-stress estimator thresholds (see insights/waterStress.js). Soil bands
    // set the base risk; the hot&dry / cool&humid pairs adjust it by evaporative
    // demand. All tunable per-crop from the Settings page.
    waterStress: {
      soilMediumBelow: clampPercent(process.env.WATER_STRESS_SOIL_MEDIUM_BELOW, 60),
      soilHighBelow: clampPercent(process.env.WATER_STRESS_SOIL_HIGH_BELOW, 30),
      hotAtOrAbove: numOr(process.env.WATER_STRESS_HOT_AT_OR_ABOVE, 33),
      dryAtOrBelow: clampPercent(process.env.WATER_STRESS_DRY_AT_OR_BELOW, 45),
      coolAtOrBelow: numOr(process.env.WATER_STRESS_COOL_AT_OR_BELOW, 22),
      humidAtOrAbove: clampPercent(process.env.WATER_STRESS_HUMID_AT_OR_ABOVE, 75),
    },
    // Canopy-coverage green-detection thresholds (HSV). Hue in DEGREES (0-360),
    // saturation/value as PERCENT (0-100). Sent to the AI /canopy endpoint.
    canopy: {
      hueMinDeg: clampRange(process.env.CANOPY_HUE_MIN_DEG, 60, 0, 360),
      hueMaxDeg: clampRange(process.env.CANOPY_HUE_MAX_DEG, 170, 0, 360),
      satMinPct: clampPercent(process.env.CANOPY_SAT_MIN_PCT, 20),
      valMinPct: clampPercent(process.env.CANOPY_VAL_MIN_PCT, 15),
    },
  };
}

function clampRange(raw, fallback, lo, hi) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  return Math.min(hi, Math.max(lo, Math.round(n)));
}

function numOr(raw, fallback) {
  const n = Number(raw);
  return Number.isFinite(n) ? n : fallback;
}

function clampMinutes(raw, fallback) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  return Math.min(AUTO_OFF_MAX, Math.max(AUTO_OFF_MIN, Math.round(n)));
}

function clampPercent(raw, fallback) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  return Math.min(100, Math.max(0, Math.round(n)));
}

const HHMM_RE = /^([01]\d|2[0-3]):([0-5]\d)$/;

function isValidTimezone(tz) {
  if (typeof tz !== 'string' || !tz) return false;
  try {
    // Throws RangeError for an unknown IANA zone.
    Intl.DateTimeFormat('en-US', { timeZone: tz });
    return true;
  } catch {
    return false;
  }
}

// Validate + normalize one schedule entry. Returns { ok, value } | { ok, error }.
function validateEntry(raw, index) {
  const where = `entries[${index}]`;
  if (!raw || typeof raw !== 'object') return { ok: false, error: `${where} must be an object` };
  if (typeof raw.start !== 'string' || !HHMM_RE.test(raw.start))
    return { ok: false, error: `${where}.start must be "HH:MM" (24h)` };
  const durationMinutes = Number(raw.durationMinutes);
  if (!Number.isFinite(durationMinutes) || durationMinutes < DURATION_MIN || durationMinutes > DURATION_MAX)
    return { ok: false, error: `${where}.durationMinutes must be ${DURATION_MIN}..${DURATION_MAX}` };
  if (!Array.isArray(raw.days) || raw.days.length === 0)
    return { ok: false, error: `${where}.days must be a non-empty array of weekdays 0..6` };
  const days = [];
  for (const d of raw.days) {
    const n = Number(d);
    if (!Number.isInteger(n) || n < 0 || n > 6)
      return { ok: false, error: `${where}.days entries must be integers 0..6 (Sun..Sat)` };
    if (!days.includes(n)) days.push(n);
  }
  days.sort((a, b) => a - b);
  const label = typeof raw.label === 'string' ? raw.label.trim().slice(0, 40) : '';
  // Stable id so the client can key rows + the scheduler can dedup fires.
  const id =
    typeof raw.id === 'string' && raw.id.trim()
      ? raw.id.trim().slice(0, 40)
      : `e${Date.now().toString(36)}${index}${Math.random().toString(36).slice(2, 6)}`;
  return {
    ok: true,
    value: { id, label, start: raw.start, durationMinutes: Math.round(durationMinutes), days, enabled: raw.enabled !== false },
  };
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
    irrigation: {
      ...settings.irrigation,
      entries: settings.irrigation.entries.map((e) => ({ ...e })),
    },
    waterStress: { ...settings.waterStress },
    canopy: { ...settings.canopy },
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

  if ('irrigation' in p) {
    const irr = p.irrigation || {};
    if ('auto' in irr) next.irrigation.auto = Boolean(irr.auto);
    if ('timezone' in irr) {
      if (!isValidTimezone(irr.timezone))
        return { ok: false, error: 'irrigation.timezone must be a valid IANA timezone' };
      next.irrigation.timezone = irr.timezone;
    }
    if ('moistureThreshold' in irr) {
      const n = Number(irr.moistureThreshold);
      if (!Number.isFinite(n) || n < 0 || n > 100)
        return { ok: false, error: 'irrigation.moistureThreshold must be 0..100' };
      next.irrigation.moistureThreshold = Math.round(n);
    }
    if ('entries' in irr) {
      if (!Array.isArray(irr.entries))
        return { ok: false, error: 'irrigation.entries must be an array' };
      const validated = [];
      for (let i = 0; i < irr.entries.length; i++) {
        const res = validateEntry(irr.entries[i], i);
        if (!res.ok) return res;
        validated.push(res.value);
      }
      next.irrigation.entries = validated; // whole-array replace
    }
  }

  if ('waterStress' in p) {
    const ws = p.waterStress || {};
    const bounded = (key, lo, hi) => {
      if (!(key in ws)) return null;
      const n = Number(ws[key]);
      if (!Number.isFinite(n) || n < lo || n > hi)
        return { ok: false, error: `waterStress.${key} must be ${lo}..${hi}` };
      next.waterStress[key] = Math.round(n);
      return { ok: true };
    };
    for (const [key, lo, hi] of [
      ['soilMediumBelow', 0, 100],
      ['soilHighBelow', 0, 100],
      ['hotAtOrAbove', -20, 60],
      ['dryAtOrBelow', 0, 100],
      ['coolAtOrBelow', -20, 60],
      ['humidAtOrAbove', 0, 100],
    ]) {
      const res = bounded(key, lo, hi);
      if (res && !res.ok) return res;
    }
    // Cross-field sanity so the bands can't invert.
    if (next.waterStress.soilHighBelow >= next.waterStress.soilMediumBelow)
      return { ok: false, error: 'waterStress.soilHighBelow must be < soilMediumBelow' };
    if (next.waterStress.coolAtOrBelow >= next.waterStress.hotAtOrAbove)
      return { ok: false, error: 'waterStress.coolAtOrBelow must be < hotAtOrAbove' };
    if (next.waterStress.dryAtOrBelow >= next.waterStress.humidAtOrAbove)
      return { ok: false, error: 'waterStress.dryAtOrBelow must be < humidAtOrAbove' };
  }

  if ('canopy' in p) {
    const cp = p.canopy || {};
    for (const [key, lo, hi] of [
      ['hueMinDeg', 0, 360],
      ['hueMaxDeg', 0, 360],
      ['satMinPct', 0, 100],
      ['valMinPct', 0, 100],
    ]) {
      if (!(key in cp)) continue;
      const n = Number(cp[key]);
      if (!Number.isFinite(n) || n < lo || n > hi)
        return { ok: false, error: `canopy.${key} must be ${lo}..${hi}` };
      next.canopy[key] = Math.round(n);
    }
    if (next.canopy.hueMinDeg >= next.canopy.hueMaxDeg)
      return { ok: false, error: 'canopy.hueMinDeg must be < hueMaxDeg' };
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
    irrigation: {
      ...settings.irrigation,
      entries: settings.irrigation.entries.map((e) => ({ ...e })),
    },
    waterStress: { ...settings.waterStress },
    canopy: { ...settings.canopy },
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
