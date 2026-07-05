const express = require('express');
const {
  setFrame,
  getFrame,
  getFrameBySeq,
  listFrames,
  subscribe,
  status,
} = require('../store/frameStore');
const cameraConfig = require('../store/cameraConfig');

const router = express.Router();

// Reject anything bigger than a UXGA JPEG could plausibly be, so a runaway
// client can't balloon memory. Tunable via env for higher-res sensors.
const MAX_FRAME_BYTES = Number(process.env.CAMERA_MAX_FRAME_BYTES) || 2 * 1024 * 1024; // 2 MB
// The v2 camera pushes on a duty cycle, so the liveness window is derived from
// that cadence — a fixed 15s would read every snapshot camera as permanently
// offline. stale = factor x interval tolerates one dropped push but flips to
// STALE after ~2 missed in a row. The interval is the live config value, so
// changing the cadence retunes staleness automatically.
const STALE_FACTOR = Number(process.env.CAMERA_STALE_FACTOR) || 2.5;
const currentStaleMs = () =>
  Math.round(cameraConfig.get().snapshot_interval_ms * STALE_FACTOR);
const BOUNDARY = 'smartfarmframe';

// ESP32-CAM PUSH target: firmware POSTs one raw JPEG per request (see its
// pushSnapshot() -> Content-Type: image/jpeg). express.raw() is built into
// Express — no new dependency — and only parses matching content types, so the
// global express.json() in server.js leaves these bodies untouched.
router.post(
  '/frame',
  express.raw({ type: ['image/jpeg', 'application/octet-stream'], limit: MAX_FRAME_BYTES }),
  (req, res) => {
    if (!Buffer.isBuffer(req.body) || req.body.length === 0) {
      return res.status(400).json({ error: 'expected a non-empty image/jpeg body' });
    }
    const frame = setFrame(req.body);
    res.status(202).json({ bytes: frame.bytes, receivedAt: new Date(frame.receivedAt).toISOString() });
  }
);

// Latest single frame — a cache-busted snapshot for the browser or other tools.
router.get('/frame.jpg', (req, res) => {
  const frame = getFrame();
  if (!frame) return res.status(503).json({ error: 'no frame received yet' });
  res.set('Content-Type', 'image/jpeg');
  res.set('Cache-Control', 'no-store');
  res.send(frame.buf);
});

// Ring history metadata (newest first) for the dashboard timeline scrubber.
// Buffers are not included — poll this, then fetch /frames/:seq for the pixels.
router.get('/frames', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json({ frames: listFrames(), capacity: status(currentStaleMs()).ringCapacity });
});

// One historical frame by monotonic seq. 404 once it has rotated out of the ring.
router.get('/frames/:seq', (req, res) => {
  const seq = Number(req.params.seq);
  if (!Number.isInteger(seq)) {
    return res.status(400).json({ error: 'seq must be an integer' });
  }
  const frame = getFrameBySeq(seq);
  if (!frame) return res.status(404).json({ error: 'frame not in ring (evicted or never existed)' });
  res.set('Content-Type', 'image/jpeg');
  res.set('Cache-Control', 'no-store');
  res.send(frame.buf);
});

// MJPEG relay: re-streams whatever is in the in-memory slot to browsers as
// multipart/x-mixed-replace. An <img src> pointed here updates in place each
// time a new frame is pushed. All viewers share the one Buffer.
router.get('/stream', (req, res) => {
  res.writeHead(200, {
    'Content-Type': `multipart/x-mixed-replace; boundary=${BOUNDARY}`,
    'Cache-Control': 'no-store',
    Pragma: 'no-cache',
    Connection: 'close',
  });

  const send = (frame) => {
    res.write(`--${BOUNDARY}\r\n`);
    res.write('Content-Type: image/jpeg\r\n');
    res.write(`Content-Length: ${frame.bytes}\r\n\r\n`);
    res.write(frame.buf);
    res.write('\r\n');
  };

  // Prime a new viewer with the current frame so it doesn't stare at a blank
  // box until the next push (which can be seconds away at low frame rates).
  const current = getFrame();
  if (current) send(current);

  const unsubscribe = subscribe(send);
  req.on('close', unsubscribe);
});

// NOTE (camera-v2): the /live pull-proxy was retired. The v2 camera no longer
// runs a continuous :81 MJPEG stream, so there is nothing to pull. "Live" is now
// the slideshow relay of pushed snapshots (GET /stream above). See
// docs/camera-longevity-redesign.md.

router.get('/status', (req, res) => {
  res.json(status(currentStaleMs()));
});

// --- Config (camera-v2) ---------------------------------------------------
// The camera GETs this each cycle and applies deltas; the dashboard POSTs it.
// Persisted to a host-mounted JSON file (see store/cameraConfig.js).
router.get('/config', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(cameraConfig.get());
});

router.post('/config', (req, res) => {
  const result = cameraConfig.update(req.body || {});
  if (!result.ok) return res.status(400).json({ error: result.error });
  res.json(result.value);
});

module.exports = router;
