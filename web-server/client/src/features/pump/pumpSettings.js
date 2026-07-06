import { useGetSettingsQuery } from '../settings/settingsApi';

// Pump-zone config is now SERVER-owned (data/settings.json), not browser-local.
// This module keeps the usePumpSettings() hook name + return shape stable so
// consumers (PumpControlCard, IrrigationPage) don't change, but reads through
// the settingsApi query instead of localStorage. The pump TARGET + auto-off are
// read server-side by the relay (pump.js); the client sends only { state }, so
// these values are used for DISPLAY (label, url) — not sent with commands.

// Backend clamp mirrors this; keep them in sync.
export const AUTO_OFF_MIN = 1;
export const AUTO_OFF_MAX = 60;

// Placeholder returned until the settings query resolves, so consumers never
// read undefined mid-load. The server's env-seeded defaults are authoritative.
export const DEFAULT_PUMP_SETTINGS = {
  url: 'http://192.168.0.5',
  label: 'Main Pump',
  autoOffMinutes: 5,
};

export function usePumpSettings() {
  const { data } = useGetSettingsQuery();
  return data?.pump ?? DEFAULT_PUMP_SETTINGS;
}
