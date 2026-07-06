// Coarse device category used to order the dashboard widgets:
//   sensor < pump < camera < other
//
// Hardware-agnostic by design (matches the "add device types with zero code
// changes" principle): only pump/camera are keyword-special-cased (as the pump
// already is elsewhere). Any other device that reports a numeric reading counts
// as a sensor and sorts first, so new sensor types slot in without edits;
// readingless unknowns fall to "other" last.

const CATEGORY_ORDER = { sensor: 0, pump: 1, camera: 2, other: 3 };

export function deviceCategory(device) {
  const id = String(device?.device_id || '').toLowerCase();
  if (/pump/.test(id)) return 'pump';
  if (/cam/.test(id)) return 'camera';
  const metrics = device?.metrics || {};
  const hasNumericReading = Object.values(metrics).some(
    (v) => typeof v === 'number' && Number.isFinite(v),
  );
  return hasNumericReading ? 'sensor' : 'other';
}

// Stable comparator: category rank first, then device_id alphabetically so the
// order is deterministic across polls (Object.values order is not guaranteed).
export function compareDevices(a, b) {
  const rankA = CATEGORY_ORDER[deviceCategory(a)];
  const rankB = CATEGORY_ORDER[deviceCategory(b)];
  if (rankA !== rankB) return rankA - rankB;
  return String(a?.device_id ?? '').localeCompare(String(b?.device_id ?? ''));
}
