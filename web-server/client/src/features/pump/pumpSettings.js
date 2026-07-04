import { useEffect, useState } from "react";

// Pump-zone config lives in the browser (localStorage), mirroring the camera
// settings. The backend is a stateless relay, so the card sends this URL +
// auto-off duration along with each command to /api/v1/pump.

export const PUMP_SETTINGS_EVENT = "smartfarm:pump-settings-changed";
const PUMP_SETTINGS_KEY = "smartfarm.pump-settings.v1";

// Backend clamp mirrors this; keep them in sync.
export const AUTO_OFF_MIN = 1;
export const AUTO_OFF_MAX = 60;

export const DEFAULT_PUMP_SETTINGS = {
  // In dev, VITE_PUMP_URL (.env.development) points this at the Jetson SSH tunnel
  // (http://localhost:8080 -> pump). Production build falls back to the real IP.
  url: import.meta.env.VITE_PUMP_URL ?? "http://192.168.0.4",
  label: "Main Pump",
  autoOffMinutes: 5,
};

function normalizeUrl(url, fallback) {
  if (typeof url !== "string") return fallback;
  const trimmed = url.trim();
  return trimmed || fallback;
}

function normalizeLabel(label, fallback) {
  if (typeof label !== "string") return fallback;
  const trimmed = label.trim();
  return trimmed || fallback;
}

function clampMinutes(raw, fallback) {
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  return Math.min(AUTO_OFF_MAX, Math.max(AUTO_OFF_MIN, Math.round(n)));
}

function normalizeSettings(raw) {
  return {
    url: normalizeUrl(raw?.url, DEFAULT_PUMP_SETTINGS.url),
    label: normalizeLabel(raw?.label, DEFAULT_PUMP_SETTINGS.label),
    autoOffMinutes: clampMinutes(
      raw?.autoOffMinutes,
      DEFAULT_PUMP_SETTINGS.autoOffMinutes,
    ),
  };
}

export function loadPumpSettings() {
  if (typeof window === "undefined") return DEFAULT_PUMP_SETTINGS;
  try {
    const saved = window.localStorage.getItem(PUMP_SETTINGS_KEY);
    if (!saved) return DEFAULT_PUMP_SETTINGS;
    return normalizeSettings(JSON.parse(saved));
  } catch {
    return DEFAULT_PUMP_SETTINGS;
  }
}

export function savePumpSettings(nextSettings) {
  if (typeof window === "undefined") return normalizeSettings(nextSettings);
  const normalized = normalizeSettings(nextSettings);
  window.localStorage.setItem(PUMP_SETTINGS_KEY, JSON.stringify(normalized));
  window.dispatchEvent(
    new CustomEvent(PUMP_SETTINGS_EVENT, { detail: normalized }),
  );
  return normalized;
}

export function usePumpSettings() {
  const [settings, setSettings] = useState(() => loadPumpSettings());

  useEffect(() => {
    const onSettingsChange = (event) => {
      setSettings(normalizeSettings(event.detail || loadPumpSettings()));
    };
    window.addEventListener(PUMP_SETTINGS_EVENT, onSettingsChange);
    return () =>
      window.removeEventListener(PUMP_SETTINGS_EVENT, onSettingsChange);
  }, []);

  return settings;
}
