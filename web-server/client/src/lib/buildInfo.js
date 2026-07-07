/* global __BUILD_INFO__ */
// Build metadata baked into the bundle by Vite's `define` (see vite.config.js):
// { version, buildNumber, gitSha, buildDate }. buildDate is an ISO-8601 UTC
// string. The Settings page's About panel renders it localized.
export const buildInfo = __BUILD_INFO__;

// Proprietary copyright shown in the UI (footer + Settings → About). Matches the
// repo-root LICENSE (all rights reserved).
export const COPYRIGHT =
  '© 2026 Watcharin Sarachai and Maejo University. All rights reserved.';

// Localized, human-readable build timestamp (e.g. "Jul 6, 2026, 2:32 PM").
// Falls back to the raw string if it isn't a parseable date.
export function formatBuildDate(iso = buildInfo.buildDate) {
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return String(iso);
  return d.toLocaleString(undefined, {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
    hour: 'numeric',
    minute: '2-digit',
  });
}
