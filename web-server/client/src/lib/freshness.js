// Derive an online/stale/offline status from a device's lastSeen timestamp.
// Devices report at their own cadence; these thresholds are deliberate defaults.
export const FRESH_MS = 30 * 1000; // <= 30s : online
export const STALE_MS = 5 * 60 * 1000; // <= 5min : stale, beyond : offline

export function freshness(lastSeen) {
  if (!lastSeen) return 'offline';
  const age = Date.now() - new Date(lastSeen).getTime();
  if (Number.isNaN(age)) return 'offline';
  if (age <= FRESH_MS) return 'online';
  if (age <= STALE_MS) return 'stale';
  return 'offline';
}
