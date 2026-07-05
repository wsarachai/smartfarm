// In-memory ring buffer of recent ESP32-CAM JPEG frames.
//
// v2: keeps the last N frames (default 90) in RAM keyed by a monotonic `seq`,
// plus a fast pointer to the `latest` for /frame.jpg and the /stream slideshow.
// Nothing is written to disk — the ring is bounded and resets on restart by
// design (matches the project's no-SD-wear principle). N x max-frame-bytes is
// the memory ceiling, so keep the framesize modest or N small on big sensors.
// MJPEG viewers subscribe and are handed the same shared Buffer, so N browsers
// cost no extra frame memory.

const RING_SIZE = Math.max(1, Number(process.env.CAMERA_RING_SIZE) || 90);

let seqCounter = 0;
let latest = null; // { seq, buf, bytes, receivedAt }
const ring = []; // FIFO, oldest first, length <= RING_SIZE
const subscribers = new Set(); // (frame) => void, one per live MJPEG client

function setFrame(buf) {
  seqCounter += 1;
  latest = { seq: seqCounter, buf, bytes: buf.length, receivedAt: Date.now() };
  ring.push(latest);
  while (ring.length > RING_SIZE) ring.shift(); // evict oldest past capacity
  for (const send of subscribers) {
    try {
      send(latest);
    } catch {
      // A dead socket throws on write; its own 'close' handler unsubscribes it.
    }
  }
  return latest;
}

function getFrame() {
  return latest;
}

// Fetch a specific frame still resident in the ring, or null if evicted.
function getFrameBySeq(seq) {
  return ring.find((f) => f.seq === seq) || null;
}

// Metadata only (no Buffers), newest first — cheap enough for the dashboard to
// poll for the timeline scrubber.
function listFrames() {
  const out = [];
  for (let i = ring.length - 1; i >= 0; i--) {
    const f = ring[i];
    out.push({ seq: f.seq, receivedAt: new Date(f.receivedAt).toISOString(), bytes: f.bytes });
  }
  return out;
}

function subscribe(fn) {
  subscribers.add(fn);
  return () => subscribers.delete(fn);
}

function status(staleMs) {
  if (!latest) {
    return {
      online: false,
      hasFrame: false,
      ageMs: null,
      bytes: 0,
      receivedAt: null,
      clients: subscribers.size,
      ringSize: ring.length,
      ringCapacity: RING_SIZE,
    };
  }
  const ageMs = Date.now() - latest.receivedAt;
  return {
    online: ageMs <= staleMs,
    hasFrame: true,
    ageMs,
    bytes: latest.bytes,
    receivedAt: new Date(latest.receivedAt).toISOString(),
    clients: subscribers.size,
    ringSize: ring.length,
    ringCapacity: RING_SIZE,
  };
}

module.exports = { setFrame, getFrame, getFrameBySeq, listFrames, subscribe, status, RING_SIZE };
