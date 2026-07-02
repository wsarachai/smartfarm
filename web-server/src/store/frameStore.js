// In-memory single-slot store for the latest ESP32-CAM JPEG frame.
//
// Exactly one Buffer is kept at a time (overwritten on every push), so memory
// stays bounded no matter how long the camera runs — and nothing is ever
// written to the Jetson's SD card. MJPEG viewers subscribe and are handed the
// same shared Buffer, so N browsers cost no extra frame memory.

let latest = null; // { buf, bytes, receivedAt }
const subscribers = new Set(); // (frame) => void, one per live MJPEG client

function setFrame(buf) {
  latest = { buf, bytes: buf.length, receivedAt: Date.now() };
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

function subscribe(fn) {
  subscribers.add(fn);
  return () => subscribers.delete(fn);
}

function status(staleMs) {
  if (!latest) {
    return { online: false, hasFrame: false, ageMs: null, bytes: 0, receivedAt: null, clients: subscribers.size };
  }
  const ageMs = Date.now() - latest.receivedAt;
  return {
    online: ageMs <= staleMs,
    hasFrame: true,
    ageMs,
    bytes: latest.bytes,
    receivedAt: new Date(latest.receivedAt).toISOString(),
    clients: subscribers.size,
  };
}

module.exports = { setFrame, getFrame, subscribe, status };
