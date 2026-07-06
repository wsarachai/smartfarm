// Water-stress estimator — a transparent, rule-based advisory (no ML model).
//
// Risk = Low / Medium / High (or Unknown) from live sensor telemetry we already
// have: soil moisture sets a base band; a hot&dry / cool&humid check adjusts it
// by evaporative demand. Soil moisture is REQUIRED and must be FRESH; temp +
// humidity are optional. Runs entirely server-side. The current value is
// recomputed on a fast live tick (smoothed in RAM); a coarser cadence appends a
// point to the persisted history (waterStressStore) for the trend.
//
// Advisory only: this never actuates the pump or the schedule.

const settingsStore = require('./../store/settingsStore');
const { listDevices } = require('./../store/deviceStore');
const waterStressStore = require('./../store/waterStressStore');

const STALE_MS = Number(process.env.WATER_STRESS_STALE_MS) || 5 * 60 * 1000;
const SMOOTH_N = Math.max(1, Number(process.env.WATER_STRESS_SMOOTH_SAMPLES) || 5);
const LIVE_MS = Number(process.env.WATER_STRESS_LIVE_MS) || 60 * 1000;
const HISTORY_MS = Number(process.env.WATER_STRESS_HISTORY_MS) || 5 * 60 * 1000;

const RISK_BY_BAND = { 1: 'low', 2: 'medium', 3: 'high' };

let ring = []; // recent numeric bands (maintained by the tick) for smoothing
let lastHistoryAt = 0;
let timer = null;

function round1(n) {
  return n == null ? null : Math.round(n * 10) / 10;
}

// Average a metric across FRESH sensor nodes (device seen within STALE_MS).
function freshAvg(devices, key) {
  const now = Date.now();
  const vals = devices
    .filter((d) => d.type !== 'actuator')
    .filter((d) => d.lastSeen && now - Date.parse(d.lastSeen) <= STALE_MS)
    .map((d) => d.metrics?.[key])
    .filter((v) => typeof v === 'number' && Number.isFinite(v));
  if (vals.length === 0) return null;
  return vals.reduce((a, b) => a + b, 0) / vals.length;
}

// Instantaneous (un-smoothed) estimate: { band, risk, inputs, factors }.
function computeInstant() {
  const cfg = settingsStore.get().waterStress;
  const devices = listDevices();
  const soil = freshAvg(devices, 'soil_moisture');
  const temp = freshAvg(devices, 'temperature');
  const humidity = freshAvg(devices, 'humidity');
  const inputs = { soilMoisture: round1(soil), temperature: round1(temp), humidity: round1(humidity) };

  if (soil === null) {
    return {
      band: null,
      risk: 'unknown',
      inputs,
      factors: ['No fresh soil-moisture reading — cannot estimate water stress.'],
    };
  }

  let band = soil < cfg.soilHighBelow ? 3 : soil < cfg.soilMediumBelow ? 2 : 1;
  const factors = [`Soil moisture ${Math.round(soil)}% → base ${RISK_BY_BAND[band]}.`];

  if (temp !== null && humidity !== null) {
    if (temp >= cfg.hotAtOrAbove && humidity <= cfg.dryAtOrBelow && band < 3) {
      band += 1;
      factors.push(`Hot & dry (${Math.round(temp)}°C / ${Math.round(humidity)}%RH) raised it to ${RISK_BY_BAND[band]}.`);
    } else if (temp <= cfg.coolAtOrBelow && humidity >= cfg.humidAtOrAbove && band > 1) {
      band -= 1;
      factors.push(`Cool & humid (${Math.round(temp)}°C / ${Math.round(humidity)}%RH) lowered it to ${RISK_BY_BAND[band]}.`);
    } else {
      factors.push(`Air ${Math.round(temp)}°C / ${Math.round(humidity)}%RH — no evaporative-demand adjustment.`);
    }
  } else {
    factors.push('Temperature/humidity unavailable — using soil moisture alone.');
  }

  return { band, risk: RISK_BY_BAND[band], inputs, factors };
}

// Current (smoothed) estimate, recomputed FRESH on each call so the badge tracks
// live telemetry without waiting for the next tick. Smoothing blends the current
// instant band with the tick-maintained ring (no mutation here).
function current() {
  const instant = computeInstant();
  if (instant.band === null) {
    return { risk: 'unknown', band: null, inputs: instant.inputs, factors: instant.factors, at: new Date().toISOString() };
  }
  const bands = [...ring, instant.band];
  const smoothedBand = Math.round(bands.reduce((a, b) => a + b, 0) / bands.length);
  return {
    risk: RISK_BY_BAND[smoothedBand],
    band: smoothedBand,
    inputs: instant.inputs,
    factors: instant.factors,
    at: new Date().toISOString(),
  };
}

// The tick maintains the smoothing ring and appends a persisted history point on
// the coarse cadence — it does NOT serve reads (current() does that live).
function tick() {
  const instant = computeInstant();
  if (instant.band === null) {
    ring = []; // a data gap resets smoothing so stale bands don't linger
  } else {
    ring.push(instant.band);
    while (ring.length > SMOOTH_N) ring.shift();
  }

  const now = Date.now();
  if (now - lastHistoryAt >= HISTORY_MS) {
    lastHistoryAt = now;
    const cur = current();
    waterStressStore.append({
      at: cur.at,
      risk: cur.risk,
      band: cur.band,
      soil: cur.inputs.soilMoisture ?? null,
      temp: cur.inputs.temperature ?? null,
      humidity: cur.inputs.humidity ?? null,
    });
  }
}

function start() {
  if (timer) return;
  tick(); // seed immediately (also writes the first history point)
  timer = setInterval(() => {
    try {
      tick();
    } catch (err) {
      console.error(`[water-stress] tick error: ${err.message}`);
    }
  }, LIVE_MS);
  if (timer.unref) timer.unref();
  console.log(`[water-stress] estimator started (live ${LIVE_MS}ms, history ${HISTORY_MS}ms)`);
}

function stop() {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
}

module.exports = { start, stop, current, tick, computeInstant, _internals: { freshAvg } };
