// Per-camera health tracking + reboot decision (camera-v2).
//
// Fed by the camera's health telemetry (free_heap, uptime_s), this decides when
// to command a reboot on the camera's next /config pull. The goal is to reboot
// on *evidence of degradation* (a heap leak) before the board locks up — while
// being structurally incapable of a reboot loop:
//   * trend trigger: free heap below a fraction of the post-boot baseline
//   * min-uptime guard: never reboot a board that just booted (lets heap settle)
//   * daily cap: after N server-triggered reboots, stop and flag DEGRADING so a
//     human sees it instead of the server masking a fast leak by looping.

const HEAP_TREND_FRACTION = Number(process.env.CAMERA_HEAP_REBOOT_FRACTION) || 0.5;
const MIN_UPTIME_S = Number(process.env.CAMERA_REBOOT_MIN_UPTIME_S) || 3600;
const MAX_REBOOTS_PER_DAY = Number(process.env.CAMERA_MAX_REBOOTS_PER_DAY) || 3;

const cams = new Map(); // device_id -> state

function todayKey() {
  return new Date().toLocaleDateString('en-CA'); // YYYY-MM-DD, local (Jetson RTC)
}

function stateFor(id) {
  let s = cams.get(id);
  if (!s) {
    s = {
      baselineHeap: null, // free heap captured just after boot
      lastUptime: null,
      lastFreeHeap: null,
      rebootPending: false, // told the camera to reboot; waiting for it to happen
      rebootsToday: 0,
      degrading: false,
      dayKey: todayKey(),
    };
    cams.set(id, s);
  }
  return s;
}

// Reset the daily counters at local midnight (Jetson has a real clock).
function rollDay(s) {
  const k = todayKey();
  if (k !== s.dayKey) {
    s.dayKey = k;
    s.rebootsToday = 0;
    s.degrading = false;
  }
}

// Ingest a telemetry sample. Only tracks devices that report free_heap+uptime_s
// (i.e. cameras), so it's safe to call for every telemetry POST.
function observe(device_id, metrics) {
  if (!metrics || metrics.free_heap == null || metrics.uptime_s == null) return;
  const s = stateFor(device_id);
  rollDay(s);
  const heap = Number(metrics.free_heap);
  const uptime = Number(metrics.uptime_s);
  // Boot detection: uptime went backwards (or first-ever sample) => fresh boot.
  if (s.lastUptime == null || uptime < s.lastUptime) {
    s.baselineHeap = heap; // capture the post-boot baseline
    s.rebootPending = false; // the commanded reboot (if any) has now happened
  } else if (s.baselineHeap == null) {
    s.baselineHeap = heap;
  }
  s.lastUptime = uptime;
  s.lastFreeHeap = heap;
}

// Decide + record whether to command a reboot now. Has intentional side effects
// (marks pending, increments the daily count) because it's called exactly when
// the decision is delivered to the camera via GET /config.
function shouldReboot(device_id) {
  const s = cams.get(device_id);
  if (!s) return false;
  rollDay(s);
  if (s.rebootPending) return false; // already told it; don't double-count
  if (s.rebootsToday >= MAX_REBOOTS_PER_DAY) {
    s.degrading = true; // give up rebooting; surface it instead
    return false;
  }
  if (s.lastUptime == null || s.lastUptime < MIN_UPTIME_S) return false;
  if (s.baselineHeap == null || s.lastFreeHeap == null) return false;
  if (s.lastFreeHeap < HEAP_TREND_FRACTION * s.baselineHeap) {
    s.rebootPending = true;
    s.rebootsToday += 1;
    return true;
  }
  return false;
}

function health(device_id) {
  const s = cams.get(device_id);
  if (!s) return { degrading: false, rebootsToday: 0, baselineHeap: null, rebootPending: false };
  rollDay(s);
  return {
    degrading: s.degrading,
    rebootsToday: s.rebootsToday,
    baselineHeap: s.baselineHeap,
    rebootPending: s.rebootPending,
  };
}

module.exports = { observe, shouldReboot, health };
