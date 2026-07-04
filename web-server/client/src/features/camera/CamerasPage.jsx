import { useEffect, useRef, useState } from "react";
import { useSelector } from "react-redux";
import { Camera, Maximize2, ExternalLink, Users } from "lucide-react";
import { useGetCameraStatusQuery } from "./cameraApi";
import { selectCameraStatus } from "./cameraSlice";
import { useGetDevicesQuery } from "../devices/devicesApi";
import { selectAllDevices } from "../devices/devicesSlice";
import { metricMeta, formatMetricValue } from "../../lib/metricMeta";
import Led from "../../components/Led";
import {
  RELAY_STREAM_URL,
  RELAY_LIVE_URL,
  isSameOriginUrl,
  useCameraSettings,
} from "../settings/cameraSettings";

const STATUS_POLL_MS = 5000;
const MAX_CAPTURES = 6;

function tsName() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, "0");
  return `esp32cam-${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

function ageLabel(ms) {
  return ms == null ? "—" : `${Math.round(ms / 1000)}s ago`;
}

function withCacheBust(url) {
  const sep = url.includes("?") ? "&" : "?";
  return `${url}${sep}t=${Date.now()}`;
}

function sourceLabel(mode, streamUrl) {
  if (mode === "relay") return "ESP32-CAM (relay)";
  try {
    const parsed = new URL(streamUrl, window.location.href);
    return parsed.host || "Custom Camera";
  } catch {
    return "Custom Camera";
  }
}

// --- Live viewport ----------------------------------------------------------
function LiveViewport({
  status,
  label,
  ageMs,
  clients,
  viewportRef,
  streamUrl,
  fallbackStreamUrl,
  forceStream,
}) {
  const [imgError, setImgError] = useState(false);
  const [activeStreamUrl, setActiveStreamUrl] = useState(streamUrl);
  const hasFrame = forceStream || status !== "offline";

  useEffect(() => {
    setImgError(false);
    setActiveStreamUrl(streamUrl);
  }, [streamUrl]);

  const handleError = () => {
    if (fallbackStreamUrl && fallbackStreamUrl !== activeStreamUrl) {
      setActiveStreamUrl(fallbackStreamUrl);
      return;
    }
    setImgError(true);
  };

  return (
    <div
      ref={viewportRef}
      className="relative bg-surface-container-lowest border border-outline-variant rounded-lg overflow-hidden aspect-video group"
    >
      {hasFrame && !imgError ? (
        <img
          src={activeStreamUrl}
          alt="ESP32-CAM live stream"
          className="absolute inset-0 w-full h-full object-contain bg-black"
          onError={handleError}
        />
      ) : (
        <div className="absolute inset-0 flex items-center justify-center text-on-surface-variant font-data-mono text-xs">
          NO SIGNAL — waiting for frames
        </div>
      )}

      <div className="absolute inset-0 scanline opacity-30 pointer-events-none" />

      {/* HUD */}
      <div className="absolute top-4 left-4 flex flex-col gap-1">
        <div className="bg-black/40 backdrop-blur-md px-2 py-1 border-l-2 border-primary flex items-center gap-2">
          <Led status={status} size="w-2 h-2" />
          <span className="font-data-mono text-label-caps text-white">
            {label}
          </span>
        </div>
        <div className="bg-black/40 backdrop-blur-md px-2 py-1 border-l-2 border-secondary">
          <span className="font-data-mono text-label-caps text-white">
            CAM_ID: ESP32-CAM
          </span>
        </div>
      </div>
      <div className="absolute top-4 right-4 bg-black/40 backdrop-blur-md px-2 py-1 flex items-center gap-2">
        <Users size={14} className="text-white" />
        <span className="font-data-mono text-label-caps text-white">
          {clients ?? 0} · {ageLabel(ageMs)}
        </span>
      </div>
    </div>
  );
}

// --- Node metrics (real fields only) ---------------------------------------
function NodeMetrics({ status, ageMs, bytes, clients }) {
  const rows = [
    {
      k: "Status",
      v: status.toUpperCase(),
      cls:
        status === "online"
          ? "text-primary"
          : status === "stale"
            ? "text-tertiary"
            : "text-error",
    },
    { k: "Last Frame", v: ageLabel(ageMs), cls: "text-on-surface" },
    {
      k: "Frame Size",
      v: bytes ? `${(bytes / 1024).toFixed(0)} KB` : "—",
      cls: "text-on-surface",
    },
    { k: "Viewers", v: String(clients ?? 0), cls: "text-on-surface" },
  ];
  return (
    <div className="panel rounded-lg p-5 border-t-2 border-t-secondary">
      <h3 className="font-headline-sm text-headline-sm text-on-surface mb-4">
        Node Metrics
      </h3>
      <div className="space-y-4">
        {rows.map((r, i) => (
          <div
            key={r.k}
            className={`flex justify-between items-center ${i < rows.length - 1 ? "border-b border-outline-variant/40 pb-2" : ""}`}
          >
            <span className="font-label-caps text-label-caps text-on-surface-variant uppercase">
              {r.k}
            </span>
            <span className={`font-data-mono text-data-mono ${r.cls}`}>
              {r.v}
            </span>
          </div>
        ))}
      </div>
      <p className="mt-4 font-data-mono text-[10px] text-on-surface-variant/70">
        RSSI / CPU temp / FPS need firmware telemetry (not yet reported).
      </p>
    </div>
  );
}

// --- Recent captures (client-side session gallery) -------------------------
function RecentCaptures({ captures }) {
  return (
    <div className="panel rounded-lg p-5">
      <div className="flex justify-between items-center mb-4">
        <h3 className="font-headline-sm text-headline-sm text-on-surface">
          Recent Captures
        </h3>
        <span className="font-label-caps text-label-caps text-on-surface-variant">
          SESSION
        </span>
      </div>
      {captures.length === 0 ? (
        <p className="font-data-mono text-xs text-on-surface-variant">
          No captures yet — hit “Capture Image”.
        </p>
      ) : (
        <div className="grid grid-cols-2 gap-2">
          {captures.map((c) => (
            <a
              key={c.id}
              href={c.url}
              target="_blank"
              rel="noreferrer"
              className="aspect-square bg-surface-container-highest border border-outline-variant relative overflow-hidden group"
            >
              <img
                src={c.url}
                alt={`capture ${c.time}`}
                className="object-cover w-full h-full grayscale group-hover:grayscale-0 transition-all"
              />
              <div className="absolute bottom-1 left-1 bg-black/60 px-1">
                <span className="font-data-mono text-[8px] text-white">
                  {c.time}
                </span>
              </div>
            </a>
          ))}
        </div>
      )}
    </div>
  );
}

// --- Sensor chips (real devices) -------------------------------------------
function SensorChips({ devices }) {
  const chips = [];
  for (const d of devices.filter((x) => x.type !== "actuator")) {
    for (const [key, value] of Object.entries(d.metrics || {})) {
      if (typeof value === "number")
        chips.push({ id: `${d.device_id}::${key}`, key, value });
    }
  }
  if (chips.length === 0) return null;
  const colors = ["bg-primary", "bg-tertiary", "bg-secondary"];
  return (
    <div className="space-y-2">
      {chips.slice(0, 4).map((c, i) => {
        const { label, unit } = metricMeta(c.key);
        return (
          <div
            key={c.id}
            className="bg-surface-container-low border border-outline-variant p-3 flex items-center gap-3"
          >
            <div className={`w-1 h-8 ${colors[i % colors.length]}`} />
            <div className="min-w-0">
              <p className="font-label-caps text-label-caps text-on-surface-variant truncate">
                {label}
              </p>
              <p className="font-data-mono text-data-mono text-on-surface">
                {formatMetricValue(c.value)}
                {unit ? ` ${unit}` : ""}
              </p>
            </div>
          </div>
        );
      })}
    </div>
  );
}

export default function CamerasPage() {
  const cameraSettings = useCameraSettings();
  const usingRelay = cameraSettings.sourceMode !== "custom";
  // Relay mode streams same-origin through the web-server: the reliable PUSH
  // relay first, falling back to the LIVE pull proxy. Custom mode points the
  // browser straight at the configured camera URL (needs direct reachability).
  const streamUrl = usingRelay ? RELAY_STREAM_URL : cameraSettings.streamUrl;
  const fallbackStreamUrl = usingRelay ? RELAY_LIVE_URL : RELAY_STREAM_URL;
  const snapshotUrl = usingRelay
    ? "/api/v1/camera/frame.jpg"
    : cameraSettings.snapshotUrl;
  const canFetchSnapshot = isSameOriginUrl(snapshotUrl);

  useGetCameraStatusQuery(undefined, {
    pollingInterval: STATUS_POLL_MS,
    skip: !usingRelay,
  });
  useGetDevicesQuery(undefined, { pollingInterval: STATUS_POLL_MS });
  const { online, hasFrame, ageMs, bytes, clients } =
    useSelector(selectCameraStatus);
  const devices = useSelector(selectAllDevices);

  const status = usingRelay
    ? !hasFrame
      ? "offline"
      : online
        ? "online"
        : "stale"
    : "online";
  const label = usingRelay
    ? status === "online"
      ? "LIVE"
      : status === "stale"
        ? "STALE"
        : "OFFLINE"
    : "EXTERNAL";
  const sourceName = sourceLabel(cameraSettings.sourceMode, streamUrl);
  const displayAgeMs = usingRelay ? ageMs : null;
  const displayBytes = usingRelay ? bytes : 0;
  const displayClients = usingRelay ? clients : null;

  const viewportRef = useRef(null);
  const [captures, setCaptures] = useState([]);
  const [busy, setBusy] = useState(false);

  // Revoke all object URLs on unmount to avoid leaking blobs.
  const capturesRef = useRef(captures);
  capturesRef.current = captures;
  useEffect(
    () => () => capturesRef.current.forEach((c) => URL.revokeObjectURL(c.url)),
    [],
  );

  const capture = async (download) => {
    setBusy(true);
    try {
      if (!canFetchSnapshot) {
        window.open(
          withCacheBust(snapshotUrl),
          "_blank",
          "noopener,noreferrer",
        );
        return;
      }

      const res = await fetch(withCacheBust(snapshotUrl));
      if (!res.ok) throw new Error(`snapshot ${res.status}`);
      const blob = await res.blob();
      const url = URL.createObjectURL(blob);
      const name = tsName();
      const time = new Date().toLocaleTimeString([], {
        hour: "2-digit",
        minute: "2-digit",
      });
      setCaptures((prev) => {
        const next = [{ id: name, url, time }, ...prev];
        next.slice(MAX_CAPTURES).forEach((c) => URL.revokeObjectURL(c.url));
        return next.slice(0, MAX_CAPTURES);
      });
      if (download) {
        const a = document.createElement("a");
        a.href = url;
        a.download = `${name}.jpg`;
        a.click();
      }
    } catch {
      // No frame available yet — keep the UI quiet; the viewport already shows NO SIGNAL.
    } finally {
      setBusy(false);
    }
  };

  const goFullscreen = () => viewportRef.current?.requestFullscreen?.();

  return (
    <div className="grid-bg -m-margin-mobile md:-m-margin-desktop p-margin-mobile md:p-margin-desktop min-h-screen">
      {/* Status bar */}
      <div className="flex flex-wrap items-center justify-between gap-3 mb-4 bg-surface-container-low p-3 border border-outline-variant rounded-lg">
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <Led status={status} />
            <span className="font-data-mono text-label-caps text-primary uppercase">
              {status === "online" ? "System Live" : status}
            </span>
          </div>
          <div className="h-4 w-px bg-outline-variant" />
          <div className="flex flex-col">
            <span className="font-label-caps text-label-caps text-on-surface-variant uppercase">
              Source
            </span>
            <span className="font-data-mono text-data-mono text-primary">
              {sourceName}
            </span>
          </div>
        </div>
        <div className="hidden md:flex items-center gap-6">
          <div className="text-right">
            <span className="font-label-caps text-label-caps text-on-surface-variant block uppercase">
              Frame Size
            </span>
            <span className="font-data-mono text-data-mono text-on-surface">
              {displayBytes ? `${(displayBytes / 1024).toFixed(0)} KB` : "—"}
            </span>
          </div>
          <div className="text-right">
            <span className="font-label-caps text-label-caps text-on-surface-variant block uppercase">
              Viewers
            </span>
            <span className="font-data-mono text-data-mono text-on-surface">
              {displayClients ?? 0}
            </span>
          </div>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-gutter">
        {/* Viewport + actions */}
        <div className="lg:col-span-9 space-y-gutter">
          <div className="relative">
            <LiveViewport
              status={status}
              label={label}
              ageMs={displayAgeMs}
              clients={displayClients}
              viewportRef={viewportRef}
              streamUrl={streamUrl}
              fallbackStreamUrl={fallbackStreamUrl}
              forceStream
            />
            <button
              type="button"
              onClick={goFullscreen}
              title="Fullscreen"
              className="absolute bottom-4 right-4 bg-black/60 hover:bg-black/80 text-white p-2 border border-white/20 rounded backdrop-blur-sm transition-colors"
            >
              <Maximize2 size={20} />
            </button>
          </div>

          <div className="flex flex-wrap gap-gutter">
            <button
              type="button"
              onClick={() => capture(true)}
              disabled={busy || status === "offline"}
              className="flex-1 min-w-[200px] flex items-center justify-center gap-3 py-4 px-6 bg-primary text-on-primary font-headline-sm rounded hover:brightness-110 active:scale-95 transition-all disabled:opacity-40 disabled:active:scale-100"
            >
              <Camera size={20} />
              <span>Capture Image</span>
            </button>
            <a
              href={snapshotUrl}
              target="_blank"
              rel="noreferrer"
              className={`flex-1 min-w-[200px] flex items-center justify-center gap-3 py-4 px-6 border border-secondary text-secondary font-headline-sm rounded hover:bg-secondary/10 active:scale-95 transition-all ${
                status === "offline" ? "pointer-events-none opacity-40" : ""
              }`}
            >
              <ExternalLink size={20} />
              <span>Open Snapshot</span>
            </a>
          </div>
        </div>

        {/* Sidebar */}
        <div className="lg:col-span-3 space-y-gutter">
          <NodeMetrics
            status={status}
            ageMs={displayAgeMs}
            bytes={displayBytes}
            clients={displayClients}
          />
          <RecentCaptures captures={captures} />
          <SensorChips devices={devices} />
        </div>
      </div>
    </div>
  );
}
