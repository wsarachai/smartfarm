// Server-owned camera behavior config (camera-v2).
//
// The camera PULLS this each cycle (GET /api/v1/camera/config) and applies any
// deltas; the dashboard WRITES it (POST /api/v1/camera/config). Unlike frames
// (RAM-only, lose-on-restart), config is durable: it's persisted to a small JSON
// file on a host volume, written atomically (temp + rename) so the camera never
// reads a torn file. Config changes are rare (only on a dashboard edit), so this
// is not the per-sample SD hammering the no-SD-wear principle guards against.

const fs = require('fs');
const path = require('path');
const frameStore = require('./frameStore');

const CONFIG_PATH =
  process.env.CAMERA_CONFIG_PATH ||
  path.join(__dirname, '..', '..', 'data', 'camera-config.json');

const MAX_FRAME_BYTES = Number(process.env.CAMERA_MAX_FRAME_BYTES) || 2 * 1024 * 1024;
// Ceiling on total ring memory (N x max-frame-bytes) so an oversized framesize
// or N can't OOM the container. Default leaves room for N=90 x 2 MB worst case.
const RING_RAM_BUDGET_BYTES =
  Number(process.env.CAMERA_RING_RAM_BUDGET_BYTES) || 256 * 1024 * 1024;

const FRAMESIZES = ['QVGA', 'CIF', 'VGA', 'SVGA', 'XGA', 'HD', 'SXGA', 'UXGA'];

function defaults() {
  return {
    snapshot_interval_ms: Number(process.env.CAMERA_SNAPSHOT_INTERVAL_MS) || 60000,
    framesize: process.env.CAMERA_FRAMESIZE || 'SVGA',
    jpeg_quality: Number(process.env.CAMERA_JPEG_QUALITY) || 12,
    enabled: true,
    reboot_interval_hours: Number(process.env.CAMERA_REBOOT_INTERVAL_HOURS) || 24,
    ring_size: Math.max(1, Number(process.env.CAMERA_RING_SIZE) || 90),
  };
}

let config = defaults();

// Validate a partial patch against the current config. Returns
// { ok, value } on success or { ok:false, error } on the first bad field.
function validate(patch) {
  const next = { ...config };
  const p = patch || {};

  if ('snapshot_interval_ms' in p) {
    const n = Number(p.snapshot_interval_ms);
    if (!Number.isFinite(n) || n < 5000 || n > 3600000)
      return { ok: false, error: 'snapshot_interval_ms must be 5000..3600000' };
    next.snapshot_interval_ms = Math.round(n);
  }
  if ('framesize' in p) {
    if (!FRAMESIZES.includes(p.framesize))
      return { ok: false, error: `framesize must be one of ${FRAMESIZES.join(', ')}` };
    next.framesize = p.framesize;
  }
  if ('jpeg_quality' in p) {
    const n = Number(p.jpeg_quality);
    if (!Number.isFinite(n) || n < 4 || n > 63)
      return { ok: false, error: 'jpeg_quality must be 4..63' };
    next.jpeg_quality = Math.round(n);
  }
  if ('enabled' in p) {
    next.enabled = Boolean(p.enabled);
  }
  if ('reboot_interval_hours' in p) {
    const n = Number(p.reboot_interval_hours);
    if (!Number.isFinite(n) || n < 0 || n > 168)
      return { ok: false, error: 'reboot_interval_hours must be 0..168' };
    next.reboot_interval_hours = Math.round(n);
  }
  if ('ring_size' in p) {
    const n = Number(p.ring_size);
    if (!Number.isInteger(n) || n < 1)
      return { ok: false, error: 'ring_size must be an integer >= 1' };
    if (n * MAX_FRAME_BYTES > RING_RAM_BUDGET_BYTES)
      return {
        ok: false,
        error: `ring_size x max_frame_bytes (${n * MAX_FRAME_BYTES}) exceeds RAM budget (${RING_RAM_BUDGET_BYTES})`,
      };
    next.ring_size = n;
  }
  return { ok: true, value: next };
}

function apply(next) {
  config = next;
  frameStore.setCapacity(config.ring_size); // resize the ring to match
}

function atomicWrite(obj) {
  fs.mkdirSync(path.dirname(CONFIG_PATH), { recursive: true });
  const tmp = `${CONFIG_PATH}.tmp-${process.pid}`;
  fs.writeFileSync(tmp, JSON.stringify(obj, null, 2));
  fs.renameSync(tmp, CONFIG_PATH); // atomic on the same filesystem
}

// Load persisted config at boot; seed + persist defaults if absent/unreadable.
function load() {
  try {
    const parsed = JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8'));
    const res = validate(parsed);
    apply(res.ok ? res.value : defaults());
  } catch {
    apply(defaults());
    try {
      atomicWrite(config);
    } catch {
      // Read-only fs / missing volume: run from in-memory defaults.
    }
  }
  return get();
}

function get() {
  return { ...config };
}

function update(patch) {
  const res = validate(patch);
  if (!res.ok) return res;
  apply(res.value);
  atomicWrite(config);
  return { ok: true, value: get() };
}

module.exports = { load, get, update, CONFIG_PATH };
