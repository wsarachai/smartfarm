import { useEffect, useState } from "react";

export const CAMERA_SETTINGS_EVENT = "smartfarm:camera-settings-changed";
const CAMERA_SETTINGS_KEY = "smartfarm.camera-settings.v1";

export const DEFAULT_CAMERA_SETTINGS = {
  sourceMode: "custom",
  streamUrl: "http://192.168.0.3:81/stream",
  snapshotUrl: "/api/v1/camera/frame.jpg",
};

function sanitizeMode(mode) {
  return mode === "custom" ? "custom" : "relay";
}

function normalizeUrl(url, fallback) {
  if (typeof url !== "string") return fallback;
  const trimmed = url.trim();
  return trimmed || fallback;
}

function normalizeSettings(raw) {
  return {
    sourceMode: sanitizeMode(raw?.sourceMode),
    streamUrl: normalizeUrl(raw?.streamUrl, DEFAULT_CAMERA_SETTINGS.streamUrl),
    snapshotUrl: normalizeUrl(
      raw?.snapshotUrl,
      DEFAULT_CAMERA_SETTINGS.snapshotUrl,
    ),
  };
}

export function loadCameraSettings() {
  if (typeof window === "undefined") return DEFAULT_CAMERA_SETTINGS;
  try {
    const saved = window.localStorage.getItem(CAMERA_SETTINGS_KEY);
    if (!saved) return DEFAULT_CAMERA_SETTINGS;
    return normalizeSettings(JSON.parse(saved));
  } catch {
    return DEFAULT_CAMERA_SETTINGS;
  }
}

export function saveCameraSettings(nextSettings) {
  if (typeof window === "undefined") return normalizeSettings(nextSettings);
  const normalized = normalizeSettings(nextSettings);
  window.localStorage.setItem(CAMERA_SETTINGS_KEY, JSON.stringify(normalized));
  window.dispatchEvent(
    new CustomEvent(CAMERA_SETTINGS_EVENT, { detail: normalized }),
  );
  return normalized;
}

export function useCameraSettings() {
  const [settings, setSettings] = useState(() => loadCameraSettings());

  useEffect(() => {
    const onSettingsChange = (event) => {
      setSettings(normalizeSettings(event.detail || loadCameraSettings()));
    };
    window.addEventListener(CAMERA_SETTINGS_EVENT, onSettingsChange);
    return () =>
      window.removeEventListener(CAMERA_SETTINGS_EVENT, onSettingsChange);
  }, []);

  return settings;
}

export function isSameOriginUrl(url) {
  try {
    const resolved = new URL(url, window.location.href);
    return resolved.origin === window.location.origin;
  } catch {
    return true;
  }
}
