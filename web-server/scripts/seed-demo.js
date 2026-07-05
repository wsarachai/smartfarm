#!/usr/bin/env node
// Seed the in-memory device store with representative telemetry so the dashboard
// has data to render. Posts through the REAL endpoints (no mock code in the app).
// Also seeds the camera-v2 path: pushes JPEG frames into the ring and reports the
// ESP32-CAM's health telemetry so the Cameras page has live-ish content.
//
//   node scripts/seed-demo.js            # post one snapshot and exit
//   node scripts/seed-demo.js --loop     # keep updating every 5s (fluctuating
//                                         # values, so the trend chart moves)
//
// Target host via BASE env (default http://localhost:3000).

const BASE = process.env.BASE || 'http://localhost:3000';
const LOOP = process.argv.includes('--loop');
const START = Date.now();

const SENSORS = [
  { device_id: 'zone-a', base: { temperature: 30.8, humidity: 72, soil_moisture: 45 } },
  { device_id: 'zone-b', base: { temperature: 30.5, humidity: 70, soil_moisture: 35 } },
];
const ACTUATORS = [{ device_id: 'main-pump', action: { running: true, mode: 'auto' } }];

// The irrigation pump's hardware node has NO onboard sensors, so it reports but
// every reading is "n/a" (not a fabricated number). Feeds the Irrigation page's
// Node Sensors table; non-numeric values are ignored by the trend charts.
const PUMP_NODE = {
  device_id: 'main-pump-node',
  metrics: { pressure: 'n/a', flow_rate: 'n/a', temperature: 'n/a', voltage: 'n/a' },
};

// camera-v2: the ESP32-CAM's own health telemetry (heap/rssi/uptime/fw) so the
// Cameras page Node Metrics + the health/reboot tracker have data. Heap is held
// well above the reboot floor so the demo camera never trips a health reboot.
const CAMERA_DEVICE_ID = 'esp32cam';
const CAMERA_HEAP_BASE = 210000;

// A small valid 96x72 gradient JPEG — enough for the ring, slideshow, and
// scrubber to render real frames without pulling in an image encoder. Same
// bytes each tick; distinct receivedAt timestamps still exercise the history
// ring. (Verified SOI/EOI valid; regenerate with PIL if you want a new image.)
const DEMO_JPEG_B64 =
  '/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAoHBwgHBgoICAgLCgoLDhgQDg0NDh0VFhEYIx8lJCIf' +
  'IiEmKzcvJik0KSEiMEExNDk7Pj4+JS5ESUM8SDc9Pjv/2wBDAQoLCw4NDhwQEBw7KCIoOzs7Ozs7' +
  'Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozv/wAARCABIAGADASIA' +
  'AhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQA' +
  'AAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3' +
  'ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWm' +
  'p6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEA' +
  'AwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSEx' +
  'BhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElK' +
  'U1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3' +
  'uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDjVSpF' +
  'SnhKeqV6zkckJDQlPVKeEp6pWbkdsJDQlPVKeEqRUrNyOyEhgSnqlPVKkVKycjshIYFqRUpypUip' +
  'WbkdkJDFWpFSnKlSBKzcjshI55Up4SnqlSBK7nI/OoSGKlSBKcqVIErNyOyEhipUgSnKlSBKycjs' +
  'hIYqVIEp6pTwlZuR2QkNVKeEp6pT1Ws3I7ISGqlPCU9UqQLWbkdkJHOqlSBKeEp6pXa5H51CQ1Up' +
  '6pTwlPVKzcjshIaqU9Up4SpFSs3I7ISGKlSKlOCVIqVm5HZCQwJUipTglSKlZuR2QkMCVIqU9Up6' +
  'pWbkdkJHPBKeqU9UqRUrtcj86hIYEqRUpypUipWbkdkJDAlSKlPVKeqVm5HZCQxVqRUp6pTwlZuR' +
  '2QkNVaeqU9UqQJWbkdkJDAtPCU9UqRUrNyOyEjnVSpAlFFd7Z+dwZIqU8JRRWbZ2QZIqU8JRRWTZ' +
  '2QZIqVIEoorNs7IMeqVIqUUVm2dkGPVKkCUUVm2dkGf/2Q==';
const DEMO_JPEG = Buffer.from(DEMO_JPEG_B64, 'base64');

function jitter(v, amp) {
  return Math.round((v + (Math.random() - 0.5) * amp) * 10) / 10;
}

async function post(path, body) {
  try {
    const res = await fetch(`${BASE}${path}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    if (!res.ok) console.error(`POST ${path} -> ${res.status}`);
  } catch (err) {
    console.error(`POST ${path} failed: ${err.message} (is the server running at ${BASE}?)`);
  }
}

async function postFrame() {
  try {
    const res = await fetch(`${BASE}/api/v1/camera/frame`, {
      method: 'POST',
      headers: { 'Content-Type': 'image/jpeg' },
      body: DEMO_JPEG,
    });
    if (!res.ok) console.error(`POST /api/v1/camera/frame -> ${res.status}`);
  } catch (err) {
    console.error(`POST /api/v1/camera/frame failed: ${err.message}`);
  }
}

async function tick() {
  for (const s of SENSORS) {
    const metrics = Object.fromEntries(
      Object.entries(s.base).map(([key, value]) => [key, jitter(value, value * 0.05 || 0.5)])
    );
    await post('/api/v1/telemetry', { device_id: s.device_id, metrics });
  }
  for (const a of ACTUATORS) {
    await post('/api/v1/control', { device_id: a.device_id, action: a.action });
  }
  // Pump node: no sensors — still reports, but every reading is n/a.
  await post('/api/v1/telemetry', PUMP_NODE);
  // Camera: push a frame into the ring + report health telemetry.
  await postFrame();
  await post('/api/v1/telemetry', {
    device_id: CAMERA_DEVICE_ID,
    metrics: {
      free_heap: Math.round(jitter(CAMERA_HEAP_BASE, 6000)),
      rssi: Math.round(jitter(-62, 6)),
      uptime_s: Math.round((Date.now() - START) / 1000) + 5,
      fw_version: '2.0.0',
    },
  });
  console.log(
    `[seed] posted ${SENSORS.length} sensor(s) + ${ACTUATORS.length} actuator(s) + camera frame/health to ${BASE}`
  );
}

(async () => {
  await tick();
  if (LOOP) {
    setInterval(tick, 5000);
    console.log('[seed] looping every 5s — Ctrl+C to stop');
  }
})();
