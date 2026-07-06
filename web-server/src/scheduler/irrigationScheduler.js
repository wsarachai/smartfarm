// Irrigation scheduler — the AUTO-mode engine.
//
// Ticks on a fixed interval, reads the schedule from settingsStore each tick (so
// edits + the auto on/off flag apply live), and fires any enabled entry whose
// start time matches the current minute on today's weekday, in the configured
// timezone. Each fire is moisture-guarded (skip if the field is already wet) and
// dedup'd so it runs at most once per day. No catch-up: a missed minute waits
// for the next occurrence. A run turns the pump on and lets the shared auto-off
// timer end it after the entry's duration (one off-path, see pumpControl).

const pumpControl = require('./../store/pumpControl');
const settingsStore = require('./../store/settingsStore');
const { listDevices } = require('./../store/deviceStore');

const TICK_MS = Number(process.env.IRRIGATION_TICK_MS) || 20000;
const MINUTES_PER_WEEK = 7 * 24 * 60;

let timer = null;
let firedDateKey = null; // the tz-local date the firedKeys set belongs to
let firedKeys = new Set(); // `${entryId}:${start}` already fired today
let lastRun = null; // { at, entryId, label, durationMinutes, moisture }
let lastSkip = null; // { at, entryId, label, reason, moisture }

// Current wall-clock in the given IANA timezone, without tz->UTC math.
function nowInTz(tz) {
  const parts = new Intl.DateTimeFormat('en-US', {
    timeZone: tz,
    weekday: 'short',
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hourCycle: 'h23',
  }).formatToParts(new Date());
  const get = (t) => parts.find((p) => p.type === t)?.value;
  const WD = { Sun: 0, Mon: 1, Tue: 2, Wed: 3, Thu: 4, Fri: 5, Sat: 6 };
  const hour = get('hour') === '24' ? '00' : get('hour'); // guard h24 edge
  return {
    weekday: WD[get('weekday')],
    hhmm: `${hour}:${get('minute')}`,
    dateKey: `${get('year')}-${get('month')}-${get('day')}`,
  };
}

// Average soil_moisture across reporting sensor nodes, or null if none report a
// numeric reading (guard fails open in that case).
function avgSoilMoisture() {
  const values = listDevices()
    .filter((d) => d.type !== 'actuator')
    .map((d) => d.metrics?.soil_moisture)
    .filter((v) => typeof v === 'number' && Number.isFinite(v));
  if (values.length === 0) return null;
  return values.reduce((a, b) => a + b, 0) / values.length;
}

// Minutes from "now" until an entry's next start (0 == firing this minute).
function minutesUntil(entry, now) {
  const nowMin = now.weekday * 1440 + Number(now.hhmm.slice(0, 2)) * 60 + Number(now.hhmm.slice(3));
  const [h, m] = entry.start.split(':').map(Number);
  let best = null;
  for (const day of entry.days) {
    const at = day * 1440 + h * 60 + m;
    const delta = ((at - nowMin) % MINUTES_PER_WEEK + MINUTES_PER_WEEK) % MINUTES_PER_WEEK;
    if (best === null || delta < best) best = delta;
  }
  return best;
}

function computeNextRun(irr, now) {
  let soonest = null;
  for (const e of irr.entries) {
    if (!e.enabled) continue;
    let delta = minutesUntil(e, now);
    if (delta === 0) delta = MINUTES_PER_WEEK; // just fired; the *next* one
    if (!soonest || delta < soonest.inMinutes) {
      soonest = { entryId: e.id, label: e.label, start: e.start, inMinutes: delta };
    }
  }
  return soonest;
}

async function fire(entry, moisture) {
  const result = await pumpControl.command('on', { runMinutes: entry.durationMinutes });
  lastRun = {
    at: new Date().toISOString(),
    entryId: entry.id,
    label: entry.label,
    durationMinutes: entry.durationMinutes,
    moisture,
    ok: result.online === true,
  };
  if (result.online) {
    console.log(`[irrigation] run "${entry.label || entry.id}" ${entry.durationMinutes}min (moisture=${moisture ?? 'n/a'})`);
  } else {
    console.error(`[irrigation] run "${entry.label || entry.id}" FAILED: ${result.error}`);
  }
}

async function tick() {
  const irr = settingsStore.get().irrigation;
  if (!irr.auto) return; // MANUAL mode: scheduler idle

  let now;
  try {
    now = nowInTz(irr.timezone);
  } catch (err) {
    console.error(`[irrigation] bad timezone "${irr.timezone}": ${err.message}`);
    return;
  }

  // Reset the per-day dedup set when the local date rolls over.
  if (firedDateKey !== now.dateKey) {
    firedDateKey = now.dateKey;
    firedKeys = new Set();
  }

  for (const entry of irr.entries) {
    if (!entry.enabled) continue;
    if (!entry.days.includes(now.weekday)) continue;
    if (entry.start !== now.hhmm) continue;
    const key = `${entry.id}:${entry.start}`;
    if (firedKeys.has(key)) continue; // already handled this minute today
    firedKeys.add(key);

    const moisture = avgSoilMoisture();
    if (moisture !== null && moisture >= irr.moistureThreshold) {
      lastSkip = {
        at: new Date().toISOString(),
        entryId: entry.id,
        label: entry.label,
        reason: `soil moisture ${Math.round(moisture)}% >= threshold ${irr.moistureThreshold}%`,
        moisture,
      };
      console.log(`[irrigation] skip "${entry.label || entry.id}" — ${lastSkip.reason}`);
      continue;
    }
    await fire(entry, moisture);
  }
}

function start() {
  if (timer) return;
  timer = setInterval(() => {
    tick().catch((err) => console.error(`[irrigation] tick error: ${err.message}`));
  }, TICK_MS);
  if (timer.unref) timer.unref(); // don't keep the process alive on its own
  console.log(`[irrigation] scheduler started (tick ${TICK_MS}ms)`);
}

function stop() {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
}

// Runtime status for the dashboard (not persisted).
function status() {
  const irr = settingsStore.get().irrigation;
  let nextRun = null;
  try {
    nextRun = computeNextRun(irr, nowInTz(irr.timezone));
  } catch {
    nextRun = null;
  }
  return {
    auto: irr.auto,
    timezone: irr.timezone,
    moistureThreshold: irr.moistureThreshold,
    entryCount: irr.entries.length,
    nextRun,
    lastRun,
    lastSkip,
  };
}

module.exports = { start, stop, status, tick, _internals: { nowInTz, avgSoilMoisture, minutesUntil } };
