const express = require('express');
const { setFrame, getFrame, subscribe, status } = require('../store/frameStore');

const router = express.Router();

// Reject anything bigger than a UXGA JPEG could plausibly be, so a runaway
// client can't balloon memory. Tunable via env for higher-res sensors.
const MAX_FRAME_BYTES = Number(process.env.CAMERA_MAX_FRAME_BYTES) || 2 * 1024 * 1024; // 2 MB
// How long since the last frame before we call the camera "stale" vs "live".
const STALE_MS = Number(process.env.CAMERA_STALE_MS) || 15000;
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

router.get('/status', (req, res) => {
  res.json(status(STALE_MS));
});

module.exports = router;
