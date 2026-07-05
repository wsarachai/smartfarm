import { useEffect, useMemo, useState } from 'react';
import { Save, RotateCcw, Video, Power, SlidersHorizontal } from 'lucide-react';
import {
  DEFAULT_CAMERA_SETTINGS,
  loadCameraSettings,
  saveCameraSettings,
} from './cameraSettings';
import {
  DEFAULT_PUMP_SETTINGS,
  AUTO_OFF_MIN,
  AUTO_OFF_MAX,
  loadPumpSettings,
  savePumpSettings,
} from '../pump/pumpSettings';

export default function SettingsPage() {
  const initial = useMemo(() => loadCameraSettings(), []);
  const [settings, setSettings] = useState(initial);
  const [saved, setSaved] = useState(false);

  const initialPump = useMemo(() => loadPumpSettings(), []);
  const [pump, setPumpSettings] = useState(initialPump);
  const [pumpSaved, setPumpSaved] = useState(false);

  const onSave = (event) => {
    event.preventDefault();
    const normalized = saveCameraSettings(settings);
    setSettings(normalized);
    setSaved(true);
    window.setTimeout(() => setSaved(false), 2000);
  };

  const onReset = () => {
    const defaults = saveCameraSettings(DEFAULT_CAMERA_SETTINGS);
    setSettings(defaults);
    setSaved(false);
  };

  const onSavePump = (event) => {
    event.preventDefault();
    const normalized = savePumpSettings(pump);
    setPumpSettings(normalized);
    setPumpSaved(true);
    window.setTimeout(() => setPumpSaved(false), 2000);
  };

  const onResetPump = () => {
    const defaults = savePumpSettings(DEFAULT_PUMP_SETTINGS);
    setPumpSettings(defaults);
    setPumpSaved(false);
  };

  const usingRelay = settings.sourceMode === 'relay';

  // --- Camera Control (device behavior, server-owned config) ----------------
  // Distinct from Camera Source Settings above (which are browser-local display
  // prefs). This reads/writes the config the camera itself pulls each cycle.
  const FRAMESIZES = ['QVGA', 'CIF', 'VGA', 'SVGA', 'XGA', 'HD', 'SXGA', 'UXGA'];
  const [device, setDevice] = useState(null);
  const [deviceErr, setDeviceErr] = useState('');
  const [deviceSaved, setDeviceSaved] = useState(false);

  useEffect(() => {
    let active = true;
    fetch('/api/v1/camera/config')
      .then((r) => (r.ok ? r.json() : null))
      .then((cfg) => {
        if (active && cfg) setDevice(cfg);
      })
      .catch(() => {});
    return () => {
      active = false;
    };
  }, []);

  const onSaveDevice = async (event) => {
    event.preventDefault();
    setDeviceErr('');
    try {
      const res = await fetch('/api/v1/camera/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          snapshot_interval_ms: Number(device.snapshot_interval_ms),
          framesize: device.framesize,
          jpeg_quality: Number(device.jpeg_quality),
          enabled: Boolean(device.enabled),
          reboot_interval_hours: Number(device.reboot_interval_hours),
          ring_size: Number(device.ring_size),
        }),
      });
      const data = await res.json();
      if (!res.ok) {
        setDeviceErr(data.error || `HTTP ${res.status}`);
        return;
      }
      setDevice(data);
      setDeviceSaved(true);
      window.setTimeout(() => setDeviceSaved(false), 2000);
    } catch {
      setDeviceErr('Network error — is the web-server reachable?');
    }
  };

  return (
    <div className="grid-bg -m-margin-mobile md:-m-margin-desktop p-margin-mobile md:p-margin-desktop min-h-screen">
      <div className="max-w-3xl mx-auto space-y-gutter">
        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <Video size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">Camera Source Settings</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                Configure where the Cameras page reads live stream and snapshot frames.
              </p>
            </div>
          </div>

          <form className="space-y-5" onSubmit={onSave}>
            <label className="block">
              <span className="font-label-caps text-label-caps text-on-surface-variant">Source Mode</span>
              <select
                className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                value={settings.sourceMode}
                onChange={(e) => setSettings((prev) => ({ ...prev, sourceMode: e.target.value }))}
              >
                <option value="relay">Web-server relay (/api/v1/camera/*)</option>
                <option value="custom">Custom camera URL</option>
              </select>
            </label>

            <label className="block">
              <span className="font-label-caps text-label-caps text-on-surface-variant">Stream URL</span>
              <input
                type="text"
                className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                placeholder="/api/v1/camera/stream or http://esp32cam.local:81/stream"
                value={settings.streamUrl}
                onChange={(e) => setSettings((prev) => ({ ...prev, streamUrl: e.target.value }))}
              />
            </label>

            <label className="block">
              <span className="font-label-caps text-label-caps text-on-surface-variant">Snapshot URL</span>
              <input
                type="text"
                className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                placeholder="/api/v1/camera/frame.jpg or http://esp32cam.local/capture"
                value={settings.snapshotUrl}
                onChange={(e) => setSettings((prev) => ({ ...prev, snapshotUrl: e.target.value }))}
              />
            </label>

            <div className="rounded border border-outline-variant bg-surface-container-low p-3">
              <p className="font-data-mono text-[11px] text-on-surface-variant leading-relaxed">
                Relay mode works best for this project. Use custom mode when you want to read directly from
                ESP32-CAM endpoints.
              </p>
              <p className="mt-2 font-data-mono text-[11px] text-on-surface-variant leading-relaxed">
                {usingRelay
                  ? 'Current mode: relay. Dashboard status comes from /api/v1/camera/status.'
                  : 'Current mode: custom. Dashboard status is based on stream availability only.'}
              </p>
            </div>

            <div className="flex flex-wrap gap-3">
              <button
                type="submit"
                className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
              >
                <Save size={16} />
                Save Settings
              </button>
              <button
                type="button"
                onClick={onReset}
                className="inline-flex items-center gap-2 bg-surface-container-high border border-outline-variant text-on-surface px-4 py-2 rounded font-label-caps text-label-caps hover:bg-surface-container-highest"
              >
                <RotateCcw size={16} />
                Reset Defaults
              </button>
              {saved ? (
                <span className="inline-flex items-center font-data-mono text-xs text-primary">Saved</span>
              ) : null}
            </div>
          </form>
        </section>

        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <SlidersHorizontal size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">Camera Control (Device)</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                Behavior the ESP32-CAM pulls each cycle. Persisted server-side; changes apply within
                one snapshot interval.
              </p>
            </div>
          </div>

          {device === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">Loading camera config…</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveDevice}>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
                <label className="block">
                  <span className="font-label-caps text-label-caps text-on-surface-variant">
                    Snapshot interval (seconds)
                  </span>
                  <input
                    type="number"
                    min={5}
                    max={3600}
                    className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    value={Math.round(device.snapshot_interval_ms / 1000)}
                    onChange={(e) =>
                      setDevice((prev) => ({
                        ...prev,
                        snapshot_interval_ms: Math.round(Number(e.target.value) * 1000),
                      }))
                    }
                  />
                </label>

                <label className="block">
                  <span className="font-label-caps text-label-caps text-on-surface-variant">Resolution</span>
                  <select
                    className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    value={device.framesize}
                    onChange={(e) => setDevice((prev) => ({ ...prev, framesize: e.target.value }))}
                  >
                    {FRAMESIZES.map((f) => (
                      <option key={f} value={f}>
                        {f}
                      </option>
                    ))}
                  </select>
                </label>

                <label className="block">
                  <span className="font-label-caps text-label-caps text-on-surface-variant">
                    JPEG quality (4–63, lower = better)
                  </span>
                  <input
                    type="number"
                    min={4}
                    max={63}
                    className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    value={device.jpeg_quality}
                    onChange={(e) =>
                      setDevice((prev) => ({ ...prev, jpeg_quality: Number(e.target.value) }))
                    }
                  />
                </label>

                <label className="block">
                  <span className="font-label-caps text-label-caps text-on-surface-variant">
                    Local reboot fallback (hours, 0 = off)
                  </span>
                  <input
                    type="number"
                    min={0}
                    max={168}
                    className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    value={device.reboot_interval_hours}
                    onChange={(e) =>
                      setDevice((prev) => ({ ...prev, reboot_interval_hours: Number(e.target.value) }))
                    }
                  />
                </label>

                <label className="block">
                  <span className="font-label-caps text-label-caps text-on-surface-variant">
                    History ring size (frames)
                  </span>
                  <input
                    type="number"
                    min={1}
                    className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    value={device.ring_size}
                    onChange={(e) => setDevice((prev) => ({ ...prev, ring_size: Number(e.target.value) }))}
                  />
                </label>

                <label className="flex items-center gap-3 mt-6">
                  <input
                    type="checkbox"
                    className="h-4 w-4 accent-primary"
                    checked={Boolean(device.enabled)}
                    onChange={(e) => setDevice((prev) => ({ ...prev, enabled: e.target.checked }))}
                  />
                  <span className="font-label-caps text-label-caps text-on-surface-variant">
                    Snapshots enabled
                  </span>
                </label>
              </div>

              {deviceErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[11px] text-error">{deviceErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  Save Camera Config
                </button>
                {deviceSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">Saved</span>
                ) : null}
              </div>
            </form>
          )}
        </section>

        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <Power size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">Pump Control Settings</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                Configure the pump-zone node the dashboard commands via the web-server relay.
              </p>
            </div>
          </div>

          <form className="space-y-5" onSubmit={onSavePump}>
            <label className="block">
              <span className="font-label-caps text-label-caps text-on-surface-variant">Pump URL</span>
              <input
                type="text"
                className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                placeholder="http://192.168.0.4"
                value={pump.url}
                onChange={(e) => setPumpSettings((prev) => ({ ...prev, url: e.target.value }))}
              />
            </label>

            <label className="block">
              <span className="font-label-caps text-label-caps text-on-surface-variant">Label</span>
              <input
                type="text"
                className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                placeholder="Main Pump"
                value={pump.label}
                onChange={(e) => setPumpSettings((prev) => ({ ...prev, label: e.target.value }))}
              />
            </label>

            <label className="block">
              <span className="font-label-caps text-label-caps text-on-surface-variant">
                Auto-off timeout (minutes)
              </span>
              <input
                type="number"
                min={AUTO_OFF_MIN}
                max={AUTO_OFF_MAX}
                className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                value={pump.autoOffMinutes}
                onChange={(e) =>
                  setPumpSettings((prev) => ({ ...prev, autoOffMinutes: e.target.value }))
                }
              />
            </label>

            <div className="rounded border border-outline-variant bg-surface-container-low p-3">
              <p className="font-data-mono text-[11px] text-on-surface-variant leading-relaxed">
                Safety: turning the pump ON arms a server-side auto-off. The web-server switches the
                pump off after this timeout even if the dashboard is closed. Clamped to {AUTO_OFF_MIN}
                –{AUTO_OFF_MAX} minutes.
              </p>
              <p className="mt-2 font-data-mono text-[11px] text-on-surface-variant leading-relaxed">
                Reserve this node&apos;s MAC in ap-server for a stable address in the .2&ndash;.99 band.
              </p>
            </div>

            <div className="flex flex-wrap gap-3">
              <button
                type="submit"
                className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
              >
                <Save size={16} />
                Save Settings
              </button>
              <button
                type="button"
                onClick={onResetPump}
                className="inline-flex items-center gap-2 bg-surface-container-high border border-outline-variant text-on-surface px-4 py-2 rounded font-label-caps text-label-caps hover:bg-surface-container-highest"
              >
                <RotateCcw size={16} />
                Reset Defaults
              </button>
              {pumpSaved ? (
                <span className="inline-flex items-center font-data-mono text-xs text-primary">Saved</span>
              ) : null}
            </div>
          </form>
        </section>
      </div>
    </div>
  );
}
