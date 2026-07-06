import { useEffect, useState } from 'react';
import { Save, RotateCcw, Video, Power, SlidersHorizontal, Info, Droplets, Bug } from 'lucide-react';
import { buildInfo, formatBuildDate } from '../../lib/buildInfo';
import { DEFAULT_CAMERA_SETTINGS } from './cameraSettings';
import {
  DEFAULT_PUMP_SETTINGS,
  AUTO_OFF_MIN,
  AUTO_OFF_MAX,
} from '../pump/pumpSettings';
import { useGetSettingsQuery, useUpdateSettingsMutation } from './settingsApi';

export default function SettingsPage() {
  // Server-owned settings (camera source + pump). Loaded once; a save invalidates
  // the cache so every open client re-reads. Local form state is seeded from the
  // server once it arrives, then edited freely until saved.
  const { data: serverSettings } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();

  const [settings, setSettings] = useState(null);
  const [saved, setSaved] = useState(false);
  const [cameraErr, setCameraErr] = useState('');

  const [pump, setPumpSettings] = useState(null);
  const [pumpSaved, setPumpSaved] = useState(false);
  const [pumpErr, setPumpErr] = useState('');

  const [ws, setWs] = useState(null);
  const [wsSaved, setWsSaved] = useState(false);
  const [wsErr, setWsErr] = useState('');

  const [cy, setCy] = useState(null);
  const [cySaved, setCySaved] = useState(false);
  const [cyErr, setCyErr] = useState('');

  const [ds, setDs] = useState(null);
  const [dsSaved, setDsSaved] = useState(false);
  const [dsErr, setDsErr] = useState('');

  // Seed the local forms from the server the first time settings arrive.
  useEffect(() => {
    if (serverSettings?.cameraSource && settings === null) {
      setSettings(serverSettings.cameraSource);
    }
    if (serverSettings?.pump && pump === null) {
      setPumpSettings(serverSettings.pump);
    }
    if (serverSettings?.waterStress && ws === null) {
      setWs(serverSettings.waterStress);
    }
    if (serverSettings?.canopy && cy === null) {
      setCy(serverSettings.canopy);
    }
    if (serverSettings?.disease && ds === null) {
      setDs(serverSettings.disease);
    }
  }, [serverSettings, settings, pump, ws, cy, ds]);

  const onSaveDs = async (event) => {
    event.preventDefault();
    setDsErr('');
    try {
      const next = await updateSettings({
        disease: { confidenceThreshold: Number(ds.confidenceThreshold) },
      }).unwrap();
      setDs(next.disease);
      setDsSaved(true);
      window.setTimeout(() => setDsSaved(false), 2000);
    } catch (err) {
      setDsErr(err?.data?.error || 'Save failed — check the value and try again.');
    }
  };

  const onSaveCy = async (event) => {
    event.preventDefault();
    setCyErr('');
    try {
      const next = await updateSettings({
        canopy: {
          hueMinDeg: Number(cy.hueMinDeg),
          hueMaxDeg: Number(cy.hueMaxDeg),
          satMinPct: Number(cy.satMinPct),
          valMinPct: Number(cy.valMinPct),
        },
      }).unwrap();
      setCy(next.canopy);
      setCySaved(true);
      window.setTimeout(() => setCySaved(false), 2000);
    } catch (err) {
      setCyErr(err?.data?.error || 'Save failed — check the values and try again.');
    }
  };

  const onSaveWs = async (event) => {
    event.preventDefault();
    setWsErr('');
    try {
      const next = await updateSettings({
        waterStress: {
          soilMediumBelow: Number(ws.soilMediumBelow),
          soilHighBelow: Number(ws.soilHighBelow),
          hotAtOrAbove: Number(ws.hotAtOrAbove),
          dryAtOrBelow: Number(ws.dryAtOrBelow),
          coolAtOrBelow: Number(ws.coolAtOrBelow),
          humidAtOrAbove: Number(ws.humidAtOrAbove),
        },
      }).unwrap();
      setWs(next.waterStress);
      setWsSaved(true);
      window.setTimeout(() => setWsSaved(false), 2000);
    } catch (err) {
      setWsErr(err?.data?.error || 'Save failed — check the values and try again.');
    }
  };

  const onSave = async (event) => {
    event.preventDefault();
    setCameraErr('');
    try {
      const next = await updateSettings({ cameraSource: settings }).unwrap();
      setSettings(next.cameraSource);
      setSaved(true);
      window.setTimeout(() => setSaved(false), 2000);
    } catch (err) {
      setCameraErr(err?.data?.error || 'Save failed — is the web-server reachable?');
    }
  };

  const onReset = async () => {
    setCameraErr('');
    try {
      const next = await updateSettings({ cameraSource: DEFAULT_CAMERA_SETTINGS }).unwrap();
      setSettings(next.cameraSource);
      setSaved(false);
    } catch (err) {
      setCameraErr(err?.data?.error || 'Reset failed — is the web-server reachable?');
    }
  };

  const onSavePump = async (event) => {
    event.preventDefault();
    setPumpErr('');
    try {
      const next = await updateSettings({
        pump: {
          url: pump.url,
          label: pump.label,
          autoOffMinutes: Number(pump.autoOffMinutes),
        },
      }).unwrap();
      setPumpSettings(next.pump);
      setPumpSaved(true);
      window.setTimeout(() => setPumpSaved(false), 2000);
    } catch (err) {
      setPumpErr(err?.data?.error || 'Save failed — is the web-server reachable?');
    }
  };

  const onResetPump = async () => {
    setPumpErr('');
    try {
      const next = await updateSettings({ pump: DEFAULT_PUMP_SETTINGS }).unwrap();
      setPumpSettings(next.pump);
      setPumpSaved(false);
    } catch (err) {
      setPumpErr(err?.data?.error || 'Reset failed — is the web-server reachable?');
    }
  };

  const usingRelay = settings?.sourceMode === 'relay';

  // --- Camera Control (device behavior, server-owned config) ----------------
  // Distinct from Camera Source Settings above (which drive where the browser
  // reads frames). This reads/writes the config the camera itself pulls each
  // cycle (a separate file/endpoint: camera-config.json via /api/v1/camera/config).
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
                Configure where the Cameras page reads live stream and snapshot frames. Saved
                server-side and shared by every client.
              </p>
            </div>
          </div>

          {settings === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">Loading settings…</p>
          ) : (
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

              {cameraErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[11px] text-error">{cameraErr}</p>
                </div>
              ) : null}

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
          )}
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
                Configure the pump-zone node the dashboard commands via the web-server relay. Saved
                server-side and shared by every client.
              </p>
            </div>
          </div>

          {pump === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">Loading settings…</p>
          ) : (
            <form className="space-y-5" onSubmit={onSavePump}>
              <label className="block">
                <span className="font-label-caps text-label-caps text-on-surface-variant">Pump URL</span>
                <input
                  type="text"
                  className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                  placeholder="http://192.168.0.5"
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

              {pumpErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[11px] text-error">{pumpErr}</p>
                </div>
              ) : null}

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
          )}
        </section>

        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <Droplets size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">Water Stress Thresholds</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                Tune the rule-based water-stress estimate (AI Insights). Soil bands set the base risk; the
                hot/dry and cool/humid pairs adjust it by evaporative demand.
              </p>
            </div>
          </div>

          {ws === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">Loading thresholds…</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveWs}>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
                {[
                  ['soilMediumBelow', 'Soil % below → Medium', 0, 100],
                  ['soilHighBelow', 'Soil % below → High', 0, 100],
                  ['hotAtOrAbove', 'Hot at/above (°C)', -20, 60],
                  ['dryAtOrBelow', 'Dry at/below (%RH)', 0, 100],
                  ['coolAtOrBelow', 'Cool at/below (°C)', -20, 60],
                  ['humidAtOrAbove', 'Humid at/above (%RH)', 0, 100],
                ].map(([key, label, min, max]) => (
                  <label key={key} className="block">
                    <span className="font-label-caps text-label-caps text-on-surface-variant">{label}</span>
                    <input
                      type="number"
                      min={min}
                      max={max}
                      value={ws[key]}
                      onChange={(e) => setWs((prev) => ({ ...prev, [key]: e.target.value }))}
                      className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    />
                  </label>
                ))}
              </div>

              {wsErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[11px] text-error">{wsErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  Save Thresholds
                </button>
                {wsSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">Saved</span>
                ) : null}
              </div>
            </form>
          )}
        </section>

        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <Droplets size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">Canopy Detection (HSV)</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                Which pixels count as green canopy (AI Insights → Canopy). Watch the mask preview there
                while tuning. Hue in degrees (green ≈ 120°); saturation/value as %.
              </p>
            </div>
          </div>

          {cy === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">Loading thresholds…</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveCy}>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
                {[
                  ['hueMinDeg', 'Hue min (°)', 0, 360],
                  ['hueMaxDeg', 'Hue max (°)', 0, 360],
                  ['satMinPct', 'Saturation min (%)', 0, 100],
                  ['valMinPct', 'Value/brightness min (%)', 0, 100],
                ].map(([key, label, min, max]) => (
                  <label key={key} className="block">
                    <span className="font-label-caps text-label-caps text-on-surface-variant">{label}</span>
                    <input
                      type="number"
                      min={min}
                      max={max}
                      value={cy[key]}
                      onChange={(e) => setCy((prev) => ({ ...prev, [key]: e.target.value }))}
                      className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                    />
                  </label>
                ))}
              </div>

              {cyErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[11px] text-error">{cyErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  Save Thresholds
                </button>
                {cySaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">Saved</span>
                ) : null}
              </div>
            </form>
          )}
        </section>

        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <Bug size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">Disease Detection</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                On-demand PlantVillage classifier (AI Insights → Disease). Below this top-1 confidence a
                result is reported as “inconclusive”. Requires the model on smartfarm-ai.
              </p>
            </div>
          </div>

          {ds === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">Loading…</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveDs}>
              <label className="block max-w-xs">
                <span className="font-label-caps text-label-caps text-on-surface-variant">
                  Confidence threshold (%)
                </span>
                <input
                  type="number"
                  min={0}
                  max={100}
                  value={ds.confidenceThreshold}
                  onChange={(e) => setDs((prev) => ({ ...prev, confidenceThreshold: e.target.value }))}
                  className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                />
              </label>

              {dsErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[11px] text-error">{dsErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  Save
                </button>
                {dsSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">Saved</span>
                ) : null}
              </div>
            </form>
          )}
        </section>

        <section className="panel rounded-lg border border-outline-variant p-6">
          <div className="flex items-start gap-3 mb-6">
            <div className="p-2 rounded bg-primary/15 text-primary">
              <Info size={18} />
            </div>
            <div>
              <h2 className="font-headline-sm text-headline-sm text-on-surface">About / System Info</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                Build metadata baked into this dashboard at compile time.
              </p>
            </div>
          </div>

          <dl className="divide-y divide-outline-variant/40">
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">Version</dt>
              <dd className="font-data-mono text-sm text-on-surface">v{buildInfo.version}</dd>
            </div>
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">Build</dt>
              <dd className="font-data-mono text-sm text-on-surface">#{buildInfo.buildNumber}</dd>
            </div>
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">Built</dt>
              <dd
                className="font-data-mono text-sm text-on-surface"
                title={buildInfo.buildDate}
              >
                {formatBuildDate()}
              </dd>
            </div>
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">Commit</dt>
              <dd className="font-data-mono text-sm text-on-surface">{buildInfo.gitSha}</dd>
            </div>
          </dl>
        </section>
      </div>
    </div>
  );
}
