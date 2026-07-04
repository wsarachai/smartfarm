import { useMemo, useState } from 'react';
import { Save, RotateCcw, Video, Power } from 'lucide-react';
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
