import { useEffect, useState } from "react";

export const CAMERA_SETTINGS_EVENT = "smartfarm:camera-settings-changed";
const CAMERA_SETTINGS_KEY = "smartfarm.camera-settings.v1";

// Same-origin PUSH relay: replays the JPEG frames the camera POSTs to the
// server (frameStore). Reliable — it only needs the camera->server push, which
// always works — but it's paced by the camera's PUSH_INTERVAL_MS. This is the
// default the Cameras + Irrigation pages stream from.
export const RELAY_STREAM_URL = "/api/v1/camera/stream";

// Same-origin LIVE proxy: the server PULLS the camera's :81 MJPEG and fans it
// out (see src/store/cameraLive.js) — smoother/higher-fps, but it requires the
// server to reach the camera's :81 directly. Used as a fallback; make it the
// primary only where the server can pull the camera stream.
export const RELAY_LIVE_URL = "/api/v1/camera/live";

export const DEFAULT_CAMERA_SETTINGS = {
  // Default to relay so a fresh install streams through the web-server proxy.
  // Custom mode points the browser straight at a camera URL (needs direct
  // reachability); use it only when viewing from the camera's own network.
  sourceMode: "relay",
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
