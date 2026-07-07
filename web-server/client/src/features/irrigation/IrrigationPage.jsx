import { useSelector } from 'react-redux';
import { Fan, Activity } from 'lucide-react';
import { useGetDevicesQuery } from '../devices/devicesApi';
import { useGetPumpStatusQuery, useSetPumpMutation } from '../pump/pumpApi';
import { useGetSettingsQuery, useUpdateSettingsMutation } from '../settings/settingsApi';
import { selectAllDevices } from '../devices/devicesSlice';
import { selectHistory } from '../history/historySlice';
import { freshness } from '../../lib/freshness';
import { metricMeta, formatMetricValue } from '../../lib/metricMeta';
import { useT } from '../../i18n';
import Led from '../../components/Led';
import ScheduleEditor from './ScheduleEditor';
import ActivityLog from './ActivityLog';

const POLL_INTERVAL_MS = 5000;
const PUMP_ID = 'main-pump';
const PUMP_NODE_ID = 'main-pump-node';
const NODE_SENSOR_KEYS = ['pressure', 'flow_rate', 'temperature', 'voltage'];
const STREAM_URL = '/api/v1/camera/stream';

function EmptyPanel({ children }) {
  return (
    <div className="panel p-5 flex items-center justify-center text-on-surface-variant font-data-mono text-xs min-h-[120px]">
      {children}
    </div>
  );
}

function avgMetric(devices, key) {
  const values = devices.map((d) => d.metrics?.[key]).filter((v) => typeof v === 'number');
  if (values.length === 0) return null;
  return values.reduce((a, b) => a + b, 0) / values.length;
}

function HeaderChip({ label, value, unit, colorClass }) {
  return (
    <div className="bg-surface-container-low px-4 py-2 border border-outline-variant flex items-center gap-3">
      <div>
        <p className="font-label-caps text-[10px] text-on-surface-variant leading-none mb-1">{label}</p>
        <p className={`font-data-mono text-headline-sm leading-none ${colorClass}`}>
          {value == null ? '—' : `${formatMetricValue(value)}${unit ? ` ${unit}` : ''}`}
        </p>
      </div>
    </div>
  );
}

function PumpVisual({ mode, running }) {
  const t = useT();
  const spinning = mode === 'manual' && running;
  const state =
    mode === 'auto'
      ? { label: t('pumpPanel.standby'), sub: t('pumpPanel.standbySub'), color: 'text-outline-variant', hw: 'IDLE', hwClass: 'bg-outline' }
      : running
        ? { label: t('pumpPanel.running'), sub: t('pumpPanel.runningSub'), color: 'text-primary', hw: 'RUN', hwClass: 'bg-primary' }
        : { label: t('pumpPanel.stopped'), sub: t('pumpPanel.stoppedSub'), color: 'text-error', hw: 'IDLE', hwClass: 'bg-error' };

  return (
    <div className="bg-surface-container-lowest border border-outline-variant p-6 flex flex-col items-center justify-center relative overflow-hidden">
      <div
        className={`w-24 h-24 rounded-full border-4 flex items-center justify-center transition-all duration-500 ${
          spinning ? 'border-primary' : 'border-outline-variant'
        }`}
      >
        <Fan size={40} className={`${state.color} ${spinning ? 'animate-spin' : ''}`} />
      </div>
      <div className="mt-6 text-center z-10">
        <p className={`font-headline-md text-headline-md uppercase tracking-tighter ${state.color}`}>{state.label}</p>
        <p className="font-data-mono text-[12px] text-outline mt-1 italic">{state.sub}</p>
      </div>
      <div className="absolute bottom-4 right-4 flex items-center bg-surface-container-high border border-outline-variant rounded overflow-hidden">
        <div className={`w-1 self-stretch ${state.hwClass}`} />
        <span className="px-2 py-1 font-data-mono text-[10px] text-on-surface-variant">HW_ST: {state.hw}</span>
      </div>
    </div>
  );
}

function PumpControlPanel({ pump }) {
  const t = useT();
  const [setPump, { isLoading }] = useSetPumpMutation();
  // AUTO/MANUAL is now a server-global mode (settings.irrigation.auto): the
  // scheduler runs in AUTO; MANUAL pauses it. Persisted so every client agrees.
  const { data: settings } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();
  const auto = Boolean(settings?.irrigation?.auto);
  const mode = auto ? 'auto' : 'manual';
  const setAuto = (on) => updateSettings({ irrigation: { auto: on } });

  if (!pump) {
    return <EmptyPanel>{t('pumpPanel.noReporting', { id: PUMP_ID })}</EmptyPanel>;
  }

  const running = Boolean(pump.metrics?.running);
  const status = freshness(pump.lastSeen);

  // Drives the real pump through the same relay + auto-off path as the dashboard
  // card; the backend mirrors the result back into this 'main-pump' device. In
  // AUTO the server refuses manual ON (409); manual OFF is always allowed.
  const togglePump = (next) => setPump({ state: next ? 'on' : 'off' });

  return (
    <div className="bg-surface-container p-5 border border-outline-variant relative overflow-hidden">
      <div className="absolute top-0 left-0 w-full h-[2px] bg-secondary opacity-50" />
      <div className="flex items-center justify-between mb-8 flex-wrap gap-4">
        <div className="flex items-center gap-3">
          <div className="p-2 bg-surface-container-high rounded-lg">
            <Fan size={28} className="text-primary" />
          </div>
          <div>
            <h3 className="font-headline-md text-headline-md text-on-surface">{t('pumpPanel.control')}</h3>
            <div className="flex items-center gap-2">
              <Led status={status} size="w-2 h-2" />
              <p className="font-body-md text-on-surface-variant text-sm">{pump.device_id}</p>
            </div>
          </div>
        </div>
        <div className="flex p-1 bg-surface-container-lowest border border-outline-variant rounded-lg">
          <button
            type="button"
            onClick={() => setAuto(true)}
            className={`px-6 py-2 font-label-caps text-label-caps rounded-md transition-all duration-200 uppercase ${
              mode === 'auto' ? 'bg-primary text-on-primary' : 'text-on-surface-variant hover:text-on-surface'
            }`}
          >
            {t('pumpPanel.auto')}
          </button>
          <button
            type="button"
            onClick={() => setAuto(false)}
            className={`px-6 py-2 font-label-caps text-label-caps rounded-md transition-all duration-200 uppercase ${
              mode === 'manual' ? 'bg-primary text-on-primary' : 'text-on-surface-variant hover:text-on-surface'
            }`}
          >
            {t('pumpPanel.manual')}
          </button>
        </div>
      </div>
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        <div>
          <p className="font-label-caps text-label-caps text-on-surface-variant mb-4">
            {mode === 'auto' ? t('pumpPanel.overrideEmergency') : t('pumpPanel.override')}
          </p>
          <div className="flex flex-col gap-3">
            <button
              type="button"
              onClick={() => togglePump(true)}
              disabled={mode === 'auto' || isLoading}
              title={mode === 'auto' ? t('pumpPanel.switchToManual') : undefined}
              className="group flex items-center justify-between px-6 py-4 border border-outline-variant bg-surface-container-low hover:bg-primary/10 hover:border-primary transition-all active:scale-95 disabled:opacity-50 disabled:hover:bg-surface-container-low disabled:hover:border-outline-variant disabled:active:scale-100"
            >
              <span className="font-headline-sm text-on-surface">{t('pumpPanel.pumpOn')}</span>
              <span className="font-data-mono text-[10px] text-outline">CMD: 0x01</span>
            </button>
            <button
              type="button"
              onClick={() => togglePump(false)}
              disabled={isLoading}
              className="group flex items-center justify-between px-6 py-4 border border-outline-variant bg-surface-container-low hover:bg-error/10 hover:border-error transition-all active:scale-95 disabled:opacity-50"
            >
              <span className="font-headline-sm text-on-surface">{t('pumpPanel.pumpOff')}</span>
              <span className="font-data-mono text-[10px] text-outline">CMD: 0x00</span>
            </button>
          </div>
        </div>
        <PumpVisual mode={mode} running={running} />
      </div>
    </div>
  );
}

function FlowSparkline({ points }) {
  if (points.length < 2) {
    return <div className="w-24 h-12 flex items-center justify-center font-data-mono text-[9px] text-on-surface-variant">…</div>;
  }
  const recent = points.slice(-8);
  const values = recent.map((p) => p.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  return (
    <div className="w-24 h-12 flex items-end gap-1">
      {recent.map((p, i) => {
        const h = Math.max(4, Math.round(((p.value - min) / range) * 48));
        return <div key={p.t} className="flex-1 bg-secondary" style={{ height: `${h}px`, opacity: 0.3 + (i / recent.length) * 0.7 }} />;
      })}
    </div>
  );
}

function FlowMetricsCard({ node, flowPoints }) {
  const t = useT();
  const flowRate = node?.metrics?.flow_rate;
  const isNum = typeof flowRate === 'number';
  return (
    <div className="bg-surface-container p-5 border border-outline-variant">
      <h4 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <Activity size={14} />
        {t('flow.title')}
      </h4>
      <div className="flex items-end justify-between">
        <div>
          <p className="font-display-lg text-[32px] leading-none text-secondary">
            {isNum ? (
              <>
                {formatMetricValue(flowRate)} <span className="text-sm font-label-caps">L/min</span>
              </>
            ) : (
              <span className="text-on-surface-variant">n/a</span>
            )}
          </p>
          <p className="font-body-md text-outline mt-2">{t('flow.throughput')}</p>
        </div>
        <FlowSparkline points={flowPoints} />
      </div>
    </div>
  );
}

function NodeCameraPreview() {
  const t = useT();
  return (
    <div className="aspect-video bg-surface-container border border-outline-variant relative overflow-hidden group">
      <img
        src={STREAM_URL}
        alt={t('node.cameraAlt')}
        className="w-full h-full object-cover opacity-80 group-hover:scale-105 transition-transform duration-700"
      />
      <div className="absolute inset-0 bg-gradient-to-t from-background via-transparent to-transparent" />
      <div className="absolute bottom-4 left-4">
        <span className="bg-primary/20 text-primary px-2 py-1 font-data-mono text-[10px] border border-primary/30">CAM_LIVE</span>
      </div>
    </div>
  );
}

function NodeSensorsTable({ node }) {
  const t = useT();
  // This node has no onboard sensors — always list the known keys, showing the
  // real reading if one is ever reported, otherwise "n/a". Never blank out.
  const rows = NODE_SENSOR_KEYS.map((k) => {
    const meta = metricMeta(k);
    const v = node?.metrics?.[k];
    const isNum = typeof v === 'number';
    return {
      key: k,
      label: meta.labelKey ? t(meta.labelKey) : meta.label,
      display: isNum ? `${formatMetricValue(v)}${meta.unit ? ` ${meta.unit}` : ''}` : 'n/a',
      na: !isNum,
    };
  });

  return (
    <div className="bg-surface-container border border-outline-variant">
      <div className="px-5 py-4 border-b border-outline-variant bg-surface-container-high">
        <h3 className="font-label-caps text-label-caps text-on-surface">{t('node.sensors')}</h3>
      </div>
      <table className="w-full text-left border-collapse">
        <thead>
          <tr className="bg-surface-container-lowest">
            <th className="px-5 py-3 font-label-caps text-[10px] text-outline">{t('node.colSensor')}</th>
            <th className="px-5 py-3 font-label-caps text-[10px] text-outline text-right">{t('node.colValue')}</th>
          </tr>
        </thead>
        <tbody className="divide-y divide-outline-variant/20">
          {rows.map((r) => (
            <tr key={r.key} className="hover:bg-surface-container-high transition-colors">
              <td className="px-5 py-3 flex items-center gap-2">
                <div className={`w-1.5 h-1.5 rounded-full ${r.na ? 'bg-outline' : 'bg-primary'}`} />
                <span className="font-data-mono text-on-surface">{r.label}</span>
              </td>
              <td className={`px-5 py-3 text-right font-data-mono ${r.na ? 'text-on-surface-variant' : 'text-secondary'}`}>
                {r.display}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export default function IrrigationPage() {
  const t = useT();
  useGetDevicesQuery(undefined, { pollingInterval: POLL_INTERVAL_MS });
  // Poll the real pump so its state stays mirrored into the store (as main-pump)
  // while this page is open — even when the dashboard's pump card isn't mounted.
  // Target is server-owned config; the status endpoint needs no argument.
  useGetPumpStatusQuery(undefined, { pollingInterval: POLL_INTERVAL_MS });
  const devices = useSelector(selectAllDevices);
  const historyPoints = useSelector(selectHistory);

  const pump = devices.find((d) => d.device_id === PUMP_ID);
  const node = devices.find((d) => d.device_id === PUMP_NODE_ID);
  const sensors = devices.filter((d) => d.type !== 'actuator');

  const avgSoilMoisture = avgMetric(sensors, 'soil_moisture');
  const avgTemp = avgMetric(sensors, 'temperature');

  const flowKey = `${PUMP_NODE_ID}::flow_rate`;
  const flowPoints = historyPoints.filter((p) => flowKey in p.values).map((p) => ({ t: p.t, value: p.values[flowKey] }));

  return (
    <div className="grid grid-cols-1 md:grid-cols-12 gap-gutter">
      <section className="md:col-span-12 flex flex-col md:flex-row md:items-end justify-between gap-4 mb-4">
        <div>
          <div className="flex items-center gap-2 mb-1">
            <Led status="online" size="w-2.5 h-2.5" />
            <span className="font-data-mono text-primary text-[12px] uppercase tracking-widest">{t('status.systemLive')}</span>
          </div>
          <h2 className="font-display-lg text-display-lg text-on-background">{t('irrigation.pageTitle')}</h2>
        </div>
        <div className="flex gap-2">
          <HeaderChip label={t('irrigation.soilMoisture')} value={avgSoilMoisture} unit="%" colorClass="text-secondary" />
          <HeaderChip label={t('irrigation.ambientTemp')} value={avgTemp} unit="°C" colorClass="text-tertiary" />
        </div>
      </section>

      <div className="md:col-span-8 space-y-gutter">
        <PumpControlPanel pump={pump} />
        <ScheduleEditor />
        <FlowMetricsCard node={node} flowPoints={flowPoints} />
      </div>

      <div className="md:col-span-4 space-y-gutter">
        <NodeCameraPreview />
        <NodeSensorsTable node={node} />
        <ActivityLog />
      </div>
    </div>
  );
}
