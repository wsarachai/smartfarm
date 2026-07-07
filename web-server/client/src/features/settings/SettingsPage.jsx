import { useEffect, useState } from 'react';
import { Save, RotateCcw, Video, Power, SlidersHorizontal, Info, Droplets, Bug } from 'lucide-react';
import { buildInfo, formatBuildDate, COPYRIGHT } from '../../lib/buildInfo';
import { DEFAULT_CAMERA_SETTINGS } from './cameraSettings';
import {
  DEFAULT_PUMP_SETTINGS,
  AUTO_OFF_MIN,
  AUTO_OFF_MAX,
} from '../pump/pumpSettings';
import { useGetSettingsQuery, useUpdateSettingsMutation } from './settingsApi';
import { useT } from '../../i18n';

// HSV (h 0-360, s/v 0-100) -> "rgb(r, g, b)". The canopy detector works in HSV,
// so the swatch chip is computed exactly rather than approximated via CSS HSL.
function hsvToRgb(h, s, v) {
  const sn = s / 100;
  const vn = v / 100;
  const c = vn * sn;
  const hp = ((((h % 360) + 360) % 360) / 60);
  const x = c * (1 - Math.abs((hp % 2) - 1));
  let r = 0;
  let g = 0;
  let b = 0;
  if (hp < 1) [r, g, b] = [c, x, 0];
  else if (hp < 2) [r, g, b] = [x, c, 0];
  else if (hp < 3) [r, g, b] = [0, c, x];
  else if (hp < 4) [r, g, b] = [0, x, c];
  else if (hp < 5) [r, g, b] = [x, 0, c];
  else [r, g, b] = [c, 0, x];
  const m = vn - c;
  const to255 = (n) => Math.round((n + m) * 255);
  return `rgb(${to255(r)}, ${to255(g)}, ${to255(b)})`;
}

// Live example of the HSV window the four canopy inputs select. Reactive to the
// unsaved form values (`cy`). Top: full-spectrum hue bar with the [min,max]
// window bracketed. Bottom: the same hue range drawn at the chosen sat/val floor
// (the palest/darkest greens that still count). No camera frame needed.
function CanopyRangePreview({ cy, t }) {
  const hueMin = Number(cy.hueMinDeg);
  const hueMax = Number(cy.hueMaxDeg);
  const satMin = Number(cy.satMinPct);
  const valMin = Number(cy.valMinPct);
  const valid =
    [hueMin, hueMax, satMin, valMin].every(Number.isFinite) && hueMin < hueMax;

  // Reference spectrum: HSV(h,100,100) === hsl(h,100%,50%), so CSS is exact here.
  const spectrum = `linear-gradient(to right, ${Array.from(
    { length: 13 },
    (_, i) => `hsl(${i * 30}, 100%, 50%)`,
  ).join(', ')})`;

  const leftPct = valid ? (hueMin / 360) * 100 : 0;
  const widthPct = valid ? ((hueMax - hueMin) / 360) * 100 : 0;

  // Accepted-floor strip across [hueMin,hueMax] at (satMin,valMin).
  const floorGradient = valid
    ? `linear-gradient(to right, ${Array.from({ length: 9 }, (_, i) => {
        const h = hueMin + ((hueMax - hueMin) * i) / 8;
        return `${hsvToRgb(h, satMin, valMin)} ${((i / 8) * 100).toFixed(0)}%`;
      }).join(', ')})`
    : null;

  return (
    <div className="rounded border border-outline-variant bg-surface-container-low p-3 space-y-3">
      <p className="font-label-caps text-label-caps text-on-surface-variant">
        {t('settings.canopy.previewLabel')}
      </p>

      <div>
        <div className="relative h-6 rounded overflow-hidden" style={{ background: spectrum }}>
          {valid ? (
            <>
              <div className="absolute inset-y-0 left-0 bg-black/60" style={{ width: `${leftPct}%` }} />
              <div className="absolute inset-y-0 right-0 bg-black/60" style={{ left: `${leftPct + widthPct}%` }} />
              <div className="absolute inset-y-0 border-x-2 border-white" style={{ left: `${leftPct}%`, width: `${widthPct}%` }} />
            </>
          ) : null}
        </div>
        <div className="flex justify-between mt-1 font-data-mono text-[11px] text-on-surface-variant">
          <span>0°</span>
          <span>{t('settings.canopy.spectrumHint')}</span>
          <span>360°</span>
        </div>
      </div>

      {valid ? (
        <div>
          <div className="h-6 rounded" style={{ background: floorGradient }} />
          <p className="mt-1 font-data-mono text-[11px] text-on-surface-variant">
            {t('settings.canopy.floorHint', {
              hueMin: Math.round(hueMin),
              hueMax: Math.round(hueMax),
              sat: Math.round(satMin),
              val: Math.round(valMin),
            })}
          </p>
        </div>
      ) : (
        <p className="font-data-mono text-[11px] text-error">{t('settings.canopy.invalidRange')}</p>
      )}
    </div>
  );
}

export default function SettingsPage() {
  const t = useT();
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
      setDsErr(err?.data?.error || t('settings.common.errValue'));
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
      setCyErr(err?.data?.error || t('settings.common.errValues'));
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
      setWsErr(err?.data?.error || t('settings.common.errValues'));
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
      setCameraErr(err?.data?.error || t('settings.common.errReachable'));
    }
  };

  const onReset = async () => {
    setCameraErr('');
    try {
      const next = await updateSettings({ cameraSource: DEFAULT_CAMERA_SETTINGS }).unwrap();
      setSettings(next.cameraSource);
      setSaved(false);
    } catch (err) {
      setCameraErr(err?.data?.error || t('settings.common.errResetReachable'));
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
      setPumpErr(err?.data?.error || t('settings.common.errReachable'));
    }
  };

  const onResetPump = async () => {
    setPumpErr('');
    try {
      const next = await updateSettings({ pump: DEFAULT_PUMP_SETTINGS }).unwrap();
      setPumpSettings(next.pump);
      setPumpSaved(false);
    } catch (err) {
      setPumpErr(err?.data?.error || t('settings.common.errResetReachable'));
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
      setDeviceErr(t('settings.cameraDevice.networkError'));
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.cameraSource.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.cameraSource.desc')}
              </p>
            </div>
          </div>

          {settings === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">{t('settings.common.loading')}</p>
          ) : (
            <form className="space-y-5" onSubmit={onSave}>
              <label className="block">
                <span className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.cameraSource.sourceMode')}</span>
                <select
                  className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                  value={settings.sourceMode}
                  onChange={(e) => setSettings((prev) => ({ ...prev, sourceMode: e.target.value }))}
                >
                  <option value="relay">{t('settings.cameraSource.sourceRelay')}</option>
                  <option value="custom">{t('settings.cameraSource.sourceCustom')}</option>
                </select>
              </label>

              <label className="block">
                <span className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.cameraSource.streamUrl')}</span>
                <input
                  type="text"
                  className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                  placeholder="/api/v1/camera/stream or http://esp32cam.local:81/stream"
                  value={settings.streamUrl}
                  onChange={(e) => setSettings((prev) => ({ ...prev, streamUrl: e.target.value }))}
                />
              </label>

              <label className="block">
                <span className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.cameraSource.snapshotUrl')}</span>
                <input
                  type="text"
                  className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                  placeholder="/api/v1/camera/frame.jpg or http://esp32cam.local/capture"
                  value={settings.snapshotUrl}
                  onChange={(e) => setSettings((prev) => ({ ...prev, snapshotUrl: e.target.value }))}
                />
              </label>

              <div className="rounded border border-outline-variant bg-surface-container-low p-3">
                <p className="font-data-mono text-[13px] text-on-surface-variant leading-relaxed">
                  {t('settings.cameraSource.help')}
                </p>
                <p className="mt-2 font-data-mono text-[13px] text-on-surface-variant leading-relaxed">
                  {usingRelay
                    ? t('settings.cameraSource.modeRelay')
                    : t('settings.cameraSource.modeCustom')}
                </p>
              </div>

              {cameraErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[13px] text-error">{cameraErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  {t('settings.common.save')}
                </button>
                <button
                  type="button"
                  onClick={onReset}
                  className="inline-flex items-center gap-2 bg-surface-container-high border border-outline-variant text-on-surface px-4 py-2 rounded font-label-caps text-label-caps hover:bg-surface-container-highest"
                >
                  <RotateCcw size={16} />
                  {t('settings.common.reset')}
                </button>
                {saved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">{t('settings.common.saved')}</span>
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.cameraDevice.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.cameraDevice.desc')}
              </p>
            </div>
          </div>

          {device === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">{t('settings.cameraDevice.loading')}</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveDevice}>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
                <label className="block">
                  <span className="font-label-caps text-label-caps text-on-surface-variant">
                    {t('settings.cameraDevice.snapshotInterval')}
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
                  <span className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.cameraDevice.resolution')}</span>
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
                    {t('settings.cameraDevice.jpegQuality')}
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
                    {t('settings.cameraDevice.rebootFallback')}
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
                    {t('settings.cameraDevice.ringSize')}
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
                    {t('settings.cameraDevice.snapshotsEnabled')}
                  </span>
                </label>
              </div>

              {deviceErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[13px] text-error">{deviceErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  {t('settings.cameraDevice.save')}
                </button>
                {deviceSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">{t('settings.common.saved')}</span>
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.pump.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.pump.desc')}
              </p>
            </div>
          </div>

          {pump === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">{t('settings.common.loading')}</p>
          ) : (
            <form className="space-y-5" onSubmit={onSavePump}>
              <label className="block">
                <span className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.pump.url')}</span>
                <input
                  type="text"
                  className="mt-2 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
                  placeholder="http://192.168.0.5"
                  value={pump.url}
                  onChange={(e) => setPumpSettings((prev) => ({ ...prev, url: e.target.value }))}
                />
              </label>

              <label className="block">
                <span className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.pump.label')}</span>
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
                  {t('settings.pump.autoOff')}
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
                <p className="font-data-mono text-[13px] text-on-surface-variant leading-relaxed">
                  {t('settings.pump.safety', { min: AUTO_OFF_MIN, max: AUTO_OFF_MAX })}
                </p>
                <p className="mt-2 font-data-mono text-[13px] text-on-surface-variant leading-relaxed">
                  {t('settings.pump.reserveMac')}
                </p>
              </div>

              {pumpErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[13px] text-error">{pumpErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  {t('settings.common.save')}
                </button>
                <button
                  type="button"
                  onClick={onResetPump}
                  className="inline-flex items-center gap-2 bg-surface-container-high border border-outline-variant text-on-surface px-4 py-2 rounded font-label-caps text-label-caps hover:bg-surface-container-highest"
                >
                  <RotateCcw size={16} />
                  {t('settings.common.reset')}
                </button>
                {pumpSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">{t('settings.common.saved')}</span>
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.waterStress.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.waterStress.desc')}
              </p>
            </div>
          </div>

          {ws === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">{t('settings.common.loadingThresholds')}</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveWs}>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
                {[
                  ['soilMediumBelow', 0, 100],
                  ['soilHighBelow', 0, 100],
                  ['hotAtOrAbove', -20, 60],
                  ['dryAtOrBelow', 0, 100],
                  ['coolAtOrBelow', -20, 60],
                  ['humidAtOrAbove', 0, 100],
                ].map(([key, min, max]) => (
                  <label key={key} className="block">
                    <span className="font-label-caps text-label-caps text-on-surface-variant">{t(`settings.waterStress.${key}`)}</span>
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
                  <p className="font-data-mono text-[13px] text-error">{wsErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  {t('settings.waterStress.save')}
                </button>
                {wsSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">{t('settings.common.saved')}</span>
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.canopy.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.canopy.desc')}
              </p>
            </div>
          </div>

          {cy === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">{t('settings.common.loadingThresholds')}</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveCy}>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
                {[
                  ['hueMinDeg', 'hueMin', 0, 360],
                  ['hueMaxDeg', 'hueMax', 0, 360],
                  ['satMinPct', 'satMin', 0, 100],
                  ['valMinPct', 'valMin', 0, 100],
                ].map(([key, labelKey, min, max]) => (
                  <label key={key} className="block">
                    <span className="font-label-caps text-label-caps text-on-surface-variant">{t(`settings.canopy.${labelKey}`)}</span>
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

              <CanopyRangePreview cy={cy} t={t} />

              {cyErr ? (
                <div className="rounded border border-error/40 bg-error/10 p-3">
                  <p className="font-data-mono text-[13px] text-error">{cyErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  {t('settings.waterStress.save')}
                </button>
                {cySaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">{t('settings.common.saved')}</span>
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.disease.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.disease.desc')}
              </p>
            </div>
          </div>

          {ds === null ? (
            <p className="font-data-mono text-xs text-on-surface-variant">{t('settings.common.loadingShort')}</p>
          ) : (
            <form className="space-y-5" onSubmit={onSaveDs}>
              <label className="block max-w-xs">
                <span className="font-label-caps text-label-caps text-on-surface-variant">
                  {t('settings.disease.confidence')}
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
                  <p className="font-data-mono text-[13px] text-error">{dsErr}</p>
                </div>
              ) : null}

              <div className="flex flex-wrap gap-3">
                <button
                  type="submit"
                  className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110"
                >
                  <Save size={16} />
                  {t('settings.common.saveGeneric')}
                </button>
                {dsSaved ? (
                  <span className="inline-flex items-center font-data-mono text-xs text-primary">{t('settings.common.saved')}</span>
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
              <h2 className="font-headline-sm text-headline-sm text-on-surface">{t('settings.about.title')}</h2>
              <p className="mt-1 font-data-mono text-xs text-on-surface-variant">
                {t('settings.about.desc')}
              </p>
            </div>
          </div>

          <dl className="divide-y divide-outline-variant/40">
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.about.version')}</dt>
              <dd className="font-data-mono text-sm text-on-surface">v{buildInfo.version}</dd>
            </div>
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.about.build')}</dt>
              <dd className="font-data-mono text-sm text-on-surface">#{buildInfo.buildNumber}</dd>
            </div>
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.about.built')}</dt>
              <dd
                className="font-data-mono text-sm text-on-surface"
                title={buildInfo.buildDate}
              >
                {formatBuildDate()}
              </dd>
            </div>
            <div className="flex items-center justify-between py-2.5">
              <dt className="font-label-caps text-label-caps text-on-surface-variant">{t('settings.about.commit')}</dt>
              <dd className="font-data-mono text-sm text-on-surface">{buildInfo.gitSha}</dd>
            </div>
          </dl>
          <p className="mt-4 pt-4 border-t border-outline-variant/40 font-data-mono text-[13px] text-on-surface-variant/80">
            {COPYRIGHT}
          </p>
        </section>
      </div>
    </div>
  );
}
