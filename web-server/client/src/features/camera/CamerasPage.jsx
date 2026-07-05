import { useEffect, useRef, useState } from "react";
import { useSelector } from "react-redux";
import {
  Maximize2,
  ExternalLink,
  Users,
  Download,
  Radio,
  AlertTriangle,
} from "lucide-react";
import { useGetCameraStatusQuery } from "./cameraApi";
import { selectCameraStatus } from "./cameraSlice";
import { useGetDevicesQuery } from "../devices/devicesApi";
import { selectAllDevices } from "../devices/devicesSlice";
import { metricMeta, formatMetricValue } from "../../lib/metricMeta";
import Led from "../../components/Led";
import {
  RELAY_STREAM_URL,
  isSameOriginUrl,
  useCameraSettings,
} from "../settings/cameraSettings";

const STATUS_POLL_MS = 5000;
const FRAMES_POLL_MS = 5000;
const STRIP_MAX = 10;

function tsName() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, "0");
  return `esp32cam-${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

function ageLabel(ms) {
  return ms == null ? "—" : `${Math.round(ms / 1000)}s ago`;
}

function timeLabel(iso) {
  try {
    return new Date(iso).toLocaleTimeString([], {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
  } catch {
    return "—";
  }
}

function withCacheBust(url) {
  const sep = url.includes("?") ? "&" : "?";
  return `${url}${sep}t=${Date.now()}`;
}

function fmtUptime(s) {
  if (s == null) return "—";
  const sec = Number(s);
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m`;
  return `${sec}s`;
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
// Shows the live slideshow (streamUrl) unless `overrideSrc` is set — when the
// user scrubs history, the parent passes a specific frame URL to freeze on.
function LiveViewport({
  status,
  label,
  ageMs,
  clients,
  viewportRef,
  streamUrl,
  fallbackStreamUrl,
  overrideSrc,
  forceStream,
}) {
  const [imgError, setImgError] = useState(false);
  const [activeStreamUrl, setActiveStreamUrl] = useState(streamUrl);
  const hasFrame = overrideSrc || forceStream || status !== "offline";

  useEffect(() => {
    setImgError(false);
    setActiveStreamUrl(streamUrl);
  }, [streamUrl]);

  useEffect(() => {
    setImgError(false);
  }, [overrideSrc]);

  const handleError = () => {
    if (overrideSrc) {
      setImgError(true);
      return;
    }
    if (fallbackStreamUrl && fallbackStreamUrl !== activeStreamUrl) {
      setActiveStreamUrl(fallbackStreamUrl);
      return;
    }
    setImgError(true);
  };

  const src = overrideSrc || activeStreamUrl;

  return (
    <div
      ref={viewportRef}
      className="relative bg-surface-container-lowest border border-outline-variant rounded-lg overflow-hidden aspect-video group"
    >
      {hasFrame && !imgError ? (
        <img
          src={src}
          alt="ESP32-CAM"
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

// --- Timeline scrubber (server-backed history over the RAM ring) ------------
function HistoryScrubber({ frames, selectedSeq, isLive, onScrub, onLive }) {
  const count = frames.length;
  if (count === 0) {
    return (
      <div className="panel rounded-lg p-4">
        <div className="flex justify-between items-center mb-1">
          <h3 className="font-headline-sm text-headline-sm text-on-surface">
            History
          </h3>
          <span className="font-label-caps text-label-caps text-on-surface-variant">
            RING
          </span>
        </div>
        <p className="font-data-mono text-xs text-on-surface-variant">
          No frames in history yet — waiting for the camera to push.
        </p>
      </div>
    );
  }

  const liveIdx = count - 1;
  const idx = isLive
    ? liveIdx
    : Math.max(0, frames.findIndex((f) => f.seq === selectedSeq));
  const sel = frames[idx];

  // A light thumbnail strip: up to STRIP_MAX evenly-spaced frames for "scent".
  const stripCount = Math.min(STRIP_MAX, count);
  const strip = [];
  for (let i = 0; i < stripCount; i++) {
    const fi = Math.round((i * (count - 1)) / Math.max(1, stripCount - 1));
    strip.push({ f: frames[fi], fi });
  }

  return (
    <div className="panel rounded-lg p-4 space-y-3">
      <div className="flex items-center justify-between">
        <h3 className="font-headline-sm text-headline-sm text-on-surface">
          History
        </h3>
        <div className="flex items-center gap-2">
          <span className="font-data-mono text-xs text-on-surface-variant">
            {isLive ? "LIVE" : `REPLAY · ${timeLabel(sel.receivedAt)}`}
          </span>
          <button
            type="button"
            onClick={onLive}
            disabled={isLive}
            className="flex items-center gap-1 border border-primary text-primary px-3 py-1 font-label-caps text-label-caps rounded hover:bg-primary/10 disabled:opacity-40 transition-colors"
          >
            <Radio size={12} />
            LIVE
          </button>
        </div>
      </div>

      <input
        type="range"
        min={0}
        max={liveIdx}
        value={idx}
        onChange={(e) => onScrub(Number(e.target.value), frames)}
        className="w-full accent-primary"
        aria-label="Scrub camera history"
      />

      <div className="flex gap-1 overflow-x-auto">
        {strip.map(({ f, fi }) => (
          <button
            key={f.seq}
            type="button"
            onClick={() => onScrub(fi, frames)}
            title={timeLabel(f.receivedAt)}
            className={`shrink-0 border overflow-hidden ${
              f.seq === sel.seq ? "border-primary" : "border-outline-variant"
            }`}
          >
            <img
              src={`/api/v1/camera/frames/${f.seq}`}
              alt=""
              loading="lazy"
              className="h-12 w-16 object-cover grayscale"
            />
          </button>
        ))}
      </div>

      <div className="flex justify-between font-data-mono text-[10px] text-on-surface-variant">
        <span>{timeLabel(frames[0].receivedAt)}</span>
        <span>{count} frames</span>
        <span>{timeLabel(frames[liveIdx].receivedAt)}</span>
      </div>
    </div>
  );
}

// --- Node metrics (real fields only) ---------------------------------------
function NodeMetrics({ status, ageMs, bytes, clients, health }) {
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

  // Real firmware telemetry (esp32cam device card) — only rows the camera reports.
  const h = health || {};
  if (h.rssi != null)
    rows.push({ k: "RSSI", v: `${h.rssi} dBm`, cls: "text-on-surface" });
  if (h.free_heap != null)
    rows.push({
      k: "Free Heap",
      v: `${(Number(h.free_heap) / 1024).toFixed(0)} KB`,
      cls: "text-on-surface",
    });
  if (h.uptime_s != null)
    rows.push({ k: "Uptime", v: fmtUptime(h.uptime_s), cls: "text-on-surface" });
  if (h.fw_version)
    rows.push({ k: "Firmware", v: String(h.fw_version), cls: "text-on-surface" });

  const hasTelemetry = h.rssi != null || h.free_heap != null;

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
      {!hasTelemetry && (
        <p className="mt-4 font-data-mono text-[10px] text-on-surface-variant/70">
          Awaiting firmware health telemetry…
        </p>
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
  const streamUrl = usingRelay ? RELAY_STREAM_URL : cameraSettings.streamUrl;
  // camera-v2: /live is retired, so relay mode has no secondary source (it shows
  // NO SIGNAL on error). Custom mode can still fall back to the slideshow relay.
  const fallbackStreamUrl = usingRelay ? null : RELAY_STREAM_URL;
  const snapshotUrl = usingRelay
    ? "/api/v1/camera/frame.jpg"
    : cameraSettings.snapshotUrl;
  const canFetchSnapshot = isSameOriginUrl(snapshotUrl);

  useGetCameraStatusQuery(undefined, {
    pollingInterval: STATUS_POLL_MS,
    skip: !usingRelay,
  });
  useGetDevicesQuery(undefined, { pollingInterval: STATUS_POLL_MS });
  const { online, hasFrame, ageMs, bytes, clients, degrading } =
    useSelector(selectCameraStatus);
  const devices = useSelector(selectAllDevices);
  const camHealth =
    devices.find((d) => d.device_id === "esp32cam")?.metrics || {};

  const status = usingRelay
    ? !hasFrame
      ? "offline"
      : online
        ? "online"
        : "stale"
    : "online";
  const sourceName = sourceLabel(cameraSettings.sourceMode, streamUrl);
  const displayAgeMs = usingRelay ? ageMs : null;
  const displayBytes = usingRelay ? bytes : 0;
  const displayClients = usingRelay ? clients : null;

  // --- Server-backed history (RAM ring) -----------------------------------
  const [frames, setFrames] = useState([]); // oldest-first: { seq, receivedAt, bytes }
  const [selectedSeq, setSelectedSeq] = useState(null); // null = live (newest)

  useEffect(() => {
    if (!usingRelay) {
      setFrames([]);
      setSelectedSeq(null);
      return undefined;
    }
    let active = true;
    const load = async () => {
      try {
        const res = await fetch("/api/v1/camera/frames");
        if (!res.ok) return;
        const data = await res.json();
        if (!active) return;
        // API returns newest-first; scrubber wants oldest-first (left=old).
        const list = (data.frames || []).slice().reverse();
        setFrames(list);
        // If the frame we were parked on rotated out of the ring, snap to live.
        setSelectedSeq((prev) =>
          prev != null && !list.some((f) => f.seq === prev) ? null : prev,
        );
      } catch {
        /* keep last-known history on a transient fetch error */
      }
    };
    load();
    const id = setInterval(load, FRAMES_POLL_MS);
    return () => {
      active = false;
      clearInterval(id);
    };
  }, [usingRelay]);

  const isLive = selectedSeq == null;
  const overrideSrc =
    !isLive && usingRelay ? `/api/v1/camera/frames/${selectedSeq}` : null;

  const onScrub = (idx, list) => {
    const liveIdx = list.length - 1;
    setSelectedSeq(idx >= liveIdx ? null : list[idx].seq);
  };
  const onLive = () => setSelectedSeq(null);

  const label = usingRelay
    ? !isLive
      ? "REPLAY"
      : status === "online"
        ? "LIVE"
        : status === "stale"
          ? "STALE"
          : "OFFLINE"
    : "EXTERNAL";

  const viewportRef = useRef(null);
  const [busy, setBusy] = useState(false);

  // Download the current snapshot straight to disk (no in-app gallery — history
  // is server-backed now).
  const downloadCurrent = async () => {
    setBusy(true);
    try {
      if (!canFetchSnapshot) {
        window.open(withCacheBust(snapshotUrl), "_blank", "noopener,noreferrer");
        return;
      }
      const res = await fetch(withCacheBust(snapshotUrl));
      if (!res.ok) throw new Error(`snapshot ${res.status}`);
      const blob = await res.blob();
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `${tsName()}.jpg`;
      a.click();
      URL.revokeObjectURL(url);
    } catch {
      // No frame available yet — keep quiet; the viewport already shows NO SIGNAL.
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
          {usingRelay && degrading && (
            <>
              <div className="h-4 w-px bg-outline-variant" />
              <div className="flex items-center gap-2 bg-error/15 border border-error/40 px-2 py-1 rounded">
                <AlertTriangle size={14} className="text-error" />
                <span className="font-label-caps text-label-caps text-error uppercase">
                  Degrading
                </span>
              </div>
            </>
          )}
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
              overrideSrc={overrideSrc}
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

          {usingRelay && (
            <HistoryScrubber
              frames={frames}
              selectedSeq={selectedSeq}
              isLive={isLive}
              onScrub={onScrub}
              onLive={onLive}
            />
          )}

          <div className="flex flex-wrap gap-gutter">
            <button
              type="button"
              onClick={downloadCurrent}
              disabled={busy || status === "offline"}
              className="flex-1 min-w-[200px] flex items-center justify-center gap-3 py-4 px-6 bg-primary text-on-primary font-headline-sm rounded hover:brightness-110 active:scale-95 transition-all disabled:opacity-40 disabled:active:scale-100"
            >
              <Download size={20} />
              <span>Download current frame</span>
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
            health={camHealth}
          />
          <SensorChips devices={devices} />
        </div>
      </div>
    </div>
  );
}
