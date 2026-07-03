import { useSelector } from 'react-redux';
import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { Cpu, Gauge, Timer, Hash, Clock, BrainCircuit, FlaskConical } from 'lucide-react';
import { useGetAnalyticsLatestQuery } from './analyticsApi';
import { selectAnalyticsLatest, selectAnalyticsHistory } from './analyticsSlice';

const POLL_INTERVAL_MS = 5000;

// Chart palette — matched to TrendChart.jsx for visual consistency.
const AXIS = '#bbcbbb'; // on-surface-variant
const GRID = '#3d4a3e'; // outline-variant
// Tertiary (gold/warning) series so the trend visually reads as "simulated",
// distinct from the Tech-Blue series used for real device telemetry.
const SERIES = '#e0c56e';

function fmtTime(t) {
  const d = new Date(t);
  return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}:${String(
    d.getSeconds()
  ).padStart(2, '0')}`;
}

// Mirrors formatUptime in StatusHeader.jsx.
function formatUptime(seconds) {
  if (seconds == null) return '—';
  const s = Math.floor(seconds);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${s % 60}s`;
}

// A small gold "SIMULATED" badge reused per data panel — no single top badge is
// allowed to speak for the whole page; every panel with fabricated numbers wears
// its own so a value can never be mistaken for a real reading.
function SimulatedBadge() {
  return (
    <span className="ml-auto flex items-center gap-1 bg-tertiary/10 border border-tertiary/40 text-tertiary px-2 py-0.5 font-label-caps text-[9px] tracking-widest">
      <FlaskConical size={10} />
      SIMULATED
    </span>
  );
}

function StatChip({ Icon, label, value }) {
  return (
    <div className="bg-surface-container-low px-4 py-2 border border-outline-variant flex items-center gap-3">
      <Icon size={18} className="text-tertiary shrink-0" />
      <div>
        <p className="font-label-caps text-[10px] text-on-surface-variant leading-none mb-1">{label}</p>
        <p className="font-data-mono text-headline-sm leading-none text-on-surface">{value}</p>
      </div>
    </div>
  );
}

function EngineStatsPanel({ latest }) {
  const model = latest?.model;
  const inference = latest?.inference;
  return (
    <div className="panel industrial-top p-5">
      <h3 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <BrainCircuit size={14} />
        AI Engine Stats
        <SimulatedBadge />
      </h3>
      <div className="flex flex-wrap gap-2">
        <StatChip
          Icon={Gauge}
          label="GPU_CLOCK"
          value={model?.gpuClockGhz == null ? '—' : `${model.gpuClockGhz.toFixed(2)} GHz`}
        />
        <StatChip Icon={Cpu} label="MODEL_V" value={model?.version ?? '—'} />
        <StatChip
          Icon={Timer}
          label="INFERENCE"
          value={inference?.latencyMs == null ? '—' : `${inference.latencyMs.toFixed(1)} ms`}
        />
        <StatChip
          Icon={Hash}
          label="INFERENCE COUNT"
          value={inference?.count == null ? '—' : inference.count.toLocaleString()}
        />
        <StatChip Icon={Clock} label="UPTIME" value={formatUptime(latest?.uptimeSeconds)} />
      </div>
    </div>
  );
}

function ConfidenceTrendPanel() {
  const history = useSelector(selectAnalyticsHistory);
  const data = history.map((p) => ({ t: p.t, value: p.confidencePct }));

  return (
    <div className="panel industrial-top overflow-hidden relative min-h-[300px]">
      <div className="p-5 flex flex-wrap gap-2 justify-between items-center border-b border-outline-variant/30">
        <h3 className="font-headline-sm text-headline-sm text-on-background">Inference Confidence Trend</h3>
        <div className="flex items-center gap-2">
          <SimulatedBadge />
          <span className="bg-surface-container-highest px-3 py-1 text-[10px] font-data-mono text-on-surface-variant">
            {data.length} SAMPLES
          </span>
        </div>
      </div>

      <div className="h-64 w-full p-2">
        {data.length < 2 ? (
          <div className="h-full flex items-center justify-center text-on-surface-variant font-data-mono text-xs">
            Collecting samples…
          </div>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            <AreaChart data={data} margin={{ top: 10, right: 16, left: 0, bottom: 0 }}>
              <defs>
                <linearGradient id="analyticsFill" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor={SERIES} stopOpacity={0.35} />
                  <stop offset="100%" stopColor={SERIES} stopOpacity={0} />
                </linearGradient>
              </defs>
              <CartesianGrid stroke={GRID} strokeOpacity={0.5} vertical={false} />
              <XAxis
                dataKey="t"
                tickFormatter={fmtTime}
                stroke={AXIS}
                tick={{ fontSize: 10, fontFamily: 'JetBrains Mono' }}
                minTickGap={40}
              />
              <YAxis
                stroke={AXIS}
                tick={{ fontSize: 10, fontFamily: 'JetBrains Mono' }}
                width={40}
                domain={['auto', 'auto']}
              />
              <Tooltip
                labelFormatter={fmtTime}
                formatter={(v) => [`${v} %`, 'Confidence']}
                contentStyle={{
                  background: '#1e2023',
                  border: '1px solid #3d4a3e',
                  fontFamily: 'JetBrains Mono',
                  fontSize: 12,
                  color: '#e2e2e6',
                }}
              />
              <Area
                type="monotone"
                dataKey="value"
                stroke={SERIES}
                strokeWidth={2}
                fill="url(#analyticsFill)"
                isAnimationActive={false}
              />
            </AreaChart>
          </ResponsiveContainer>
        )}
      </div>
    </div>
  );
}

export default function AnalyticsPage() {
  useGetAnalyticsLatestQuery(undefined, { pollingInterval: POLL_INTERVAL_MS });
  const latest = useSelector(selectAnalyticsLatest);

  return (
    <div className="grid grid-cols-1 md:grid-cols-12 gap-gutter">
      <section className="md:col-span-12 flex flex-col md:flex-row md:items-end justify-between gap-4 mb-4">
        <div>
          {/* Deliberately NOT the Led "online" pulse — that visual language means
              "real and healthy" elsewhere. A static gold chip flags fake telemetry. */}
          <div className="flex items-center gap-2 mb-1">
            <span className="flex items-center gap-2 bg-tertiary/10 border border-tertiary/40 text-tertiary px-3 py-1 font-data-mono text-[12px] uppercase tracking-widest">
              <FlaskConical size={13} />
              Simulated Engine
            </span>
          </div>
          <h2 className="font-display-lg text-display-lg text-on-background">AI Analytics</h2>
          <p className="font-body-md text-on-surface-variant text-sm mt-1">
            Demo inference telemetry — every value is fabricated by a backend simulator, not a real model.
          </p>
        </div>
      </section>

      <div className="md:col-span-12">
        <EngineStatsPanel latest={latest} />
      </div>

      <div className="md:col-span-12">
        <ConfidenceTrendPanel />
      </div>
    </div>
  );
}
