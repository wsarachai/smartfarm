#!/usr/bin/env node
// Seed the in-memory device store with representative telemetry so the dashboard
// has data to render. Posts through the REAL endpoints (no mock code in the app).
//
//   node scripts/seed-demo.js            # post one snapshot and exit
//   node scripts/seed-demo.js --loop     # keep updating every 5s (fluctuating
//                                         # values, so the trend chart moves)
//
// Target host via BASE env (default http://localhost:3000).

const BASE = process.env.BASE || 'http://localhost:3000';
const LOOP = process.argv.includes('--loop');

const SENSORS = [
  { device_id: 'zone-a', base: { temperature: 30.8, humidity: 72, soil_moisture: 45 } },
  { device_id: 'zone-b', base: { temperature: 30.5, humidity: 70, soil_moisture: 35 } },
];
const ACTUATORS = [{ device_id: 'main-pump', action: { running: true } }];

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

async function tick() {
  for (const s of SENSORS) {
    const metrics = {
      temperature: jitter(s.base.temperature, 0.6),
      humidity: Math.round(jitter(s.base.humidity, 3)),
      soil_moisture: Math.round(jitter(s.base.soil_moisture, 4)),
    };
    await post('/api/v1/telemetry', { device_id: s.device_id, metrics });
  }
  for (const a of ACTUATORS) {
    await post('/api/v1/control', { device_id: a.device_id, action: a.action });
  }
  console.log(`[seed] posted ${SENSORS.length} sensor(s) + ${ACTUATORS.length} actuator(s) to ${BASE}`);
}

(async () => {
  await tick();
  if (LOOP) {
    setInterval(tick, 5000);
    console.log('[seed] looping every 5s — Ctrl+C to stop');
  }
})();
