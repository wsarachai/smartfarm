const http = require('http');

// Live MJPEG proxy. The web-server (which sits on the camera's Wi-Fi network)
// pulls the ESP32-CAM's own high-fps stream ONCE and fans it out to every
// dashboard viewer. This means:
//   * browsers only ever talk to the web-server (same origin) — they don't need
//     to reach the camera directly, so the live view works from any client that
//     can load the dashboard (fixes the "black on the deployed server" case);
//   * the camera sees a single connection (the web-server), sidestepping the
//     ESP32-CAM's very small concurrent-stream limit.
//
// Unlike frameStore (the PUSH path: 1 JPEG every ~10s), this relays the camera's
// continuous :81 MJPEG, so it's actually live.

const STREAM_URL = process.env.CAMERA_STREAM_URL || 'http://192.168.0.3:81/stream';
const RECONNECT_MS = Number(process.env.CAMERA_LIVE_RECONNECT_MS) || 3000;
const UPSTREAM_TIMEOUT_MS = Number(process.env.CAMERA_LIVE_TIMEOUT_MS) || 10000;

let upstream = null;        // active http.ClientRequest to the camera
let contentType = null;     // upstream multipart content-type (incl. boundary)
let reconnectTimer = null;
const viewers = new Set();  // res: headers sent, receiving chunks
const pending = new Set();  // res: waiting for the first upstream response

function sendHeaders(res) {
  res.writeHead(200, {
    'Content-Type': contentType || 'multipart/x-mixed-replace',
    'Cache-Control': 'no-store, no-cache, must-revalidate',
    Pragma: 'no-cache',
    Connection: 'close',
  });
}

// Camera never responded: end pending viewers so their <img> errors out and the
// page shows "NO SIGNAL" instead of spinning forever.
function dropPending() {
  for (const v of pending) {
    try { v.end(); } catch { /* client already gone */ }
  }
  pending.clear();
}

function scheduleReconnect() {
  if (reconnectTimer || viewers.size === 0) return;
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    startUpstream();
  }, RECONNECT_MS);
}

function onUpstreamDown() {
  if (upstream) {
    try { upstream.destroy(); } catch { /* noop */ }
    upstream = null;
  }
  contentType = null;
  // Viewers that never got a stream: tell them it's down.
  dropPending();
  // Active viewers: keep their sockets open and retry, so a transient camera
  // blip recovers without a page reload (the ESP32-CAM boundary is stable).
  if (viewers.size > 0) scheduleReconnect();
}

function startUpstream() {
  if (upstream || (viewers.size === 0 && pending.size === 0)) return;

  const req = http.get(STREAM_URL, (res) => {
    if (res.statusCode !== 200) {
      res.destroy();
      onUpstreamDown();
      return;
    }
    contentType = res.headers['content-type'] || 'multipart/x-mixed-replace';
    // Bytes are flowing — promote pending viewers to active.
    for (const v of pending) {
      sendHeaders(v);
      viewers.add(v);
    }
    pending.clear();

    res.on('data', (chunk) => {
      for (const v of viewers) {
        try { v.write(chunk); } catch { /* dropped on next close event */ }
      }
    });
    res.on('end', onUpstreamDown);
    res.on('error', onUpstreamDown);
  });

  req.on('error', onUpstreamDown);
  req.setTimeout(UPSTREAM_TIMEOUT_MS, () => req.destroy(new Error('camera stream timeout')));
  upstream = req;
}

function addViewer(res) {
  if (contentType) {
    // Upstream is already streaming — start this viewer immediately instead of
    // parking it in `pending` (whose promotion only runs on the first response).
    sendHeaders(res);
    viewers.add(res);
    return;
  }
  pending.add(res);
  startUpstream();
}

function removeViewer(res) {
  viewers.delete(res);
  pending.delete(res);
  // Nobody watching -> stop pulling from the camera.
  if (viewers.size === 0 && pending.size === 0) {
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    if (upstream) {
      try { upstream.destroy(); } catch { /* noop */ }
      upstream = null;
    }
    contentType = null;
  }
}

function viewerCount() {
  return viewers.size + pending.size;
}

module.exports = { addViewer, removeViewer, viewerCount, STREAM_URL };
