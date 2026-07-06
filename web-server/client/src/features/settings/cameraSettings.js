import { useGetSettingsQuery } from './settingsApi';

// Camera SOURCE prefs (where the browser reads live/snapshot frames) are now
// SERVER-owned (data/settings.json), not browser-local. This module keeps the
// useCameraSettings() hook name + return shape stable so consumers (CamerasPage)
// don't change, but reads through the settingsApi query instead of localStorage.
// These are browser-only hints — the server stores them but never consumes them.

// Same-origin PUSH relay: replays the JPEG frames the camera POSTs to the
// server (frameStore). Reliable — it only needs the camera->server push, which
// always works — but it's paced by the camera's PUSH_INTERVAL_MS. This is the
// default the Cameras + Irrigation pages stream from.
export const RELAY_STREAM_URL = '/api/v1/camera/stream';

// Placeholder returned until the settings query resolves, so consumers never
// read undefined mid-load. The server's env-seeded defaults are authoritative.
export const DEFAULT_CAMERA_SETTINGS = {
  sourceMode: 'relay',
  streamUrl: 'http://192.168.0.3:81/stream',
  snapshotUrl: '/api/v1/camera/frame.jpg',
};

export function useCameraSettings() {
  const { data } = useGetSettingsQuery();
  return data?.cameraSource ?? DEFAULT_CAMERA_SETTINGS;
}

export function isSameOriginUrl(url) {
  try {
    const resolved = new URL(url, window.location.href);
    return resolved.origin === window.location.origin;
  } catch {
    return true;
  }
}
