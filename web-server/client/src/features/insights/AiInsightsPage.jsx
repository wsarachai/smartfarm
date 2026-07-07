import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { BrainCircuit, Droplet, Thermometer, Waves, Info, Sprout, Bug, ScanSearch } from 'lucide-react';
import { useGetWaterStressQuery, useGetWaterStressHistoryQuery } from './waterStressApi';
import { useGetCanopyQuery, useGetCanopyHistoryQuery } from './canopyApi';
import { useGetDiseaseQuery, useGetDiseaseHistoryQuery, useAnalyzeDiseaseMutation } from './diseaseApi';
import { riskMeta } from './risk';
import { diseaseMeta } from './diseaseMeta';
import { useT } from '../../i18n';

const POLL_MS = 5000;
const AXIS = '#bbcbbb';
const GRID = '#3d4a3e';
const SERIES = '#7fb0ff'; // tech-blue: this is REAL data now, not the gold "simulated" hue

// band 1/2/3 -> chart y; unknown -> null (gap).
function bandOf(p) {
  return p.band == null ? null : p.band;
}
function fmtTime(t) {
  const d = new Date(t);
  return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
}

function StatChip({ Icon, label, value, unit }) {
  return (
    <div className="bg-surface-container-low px-4 py-2 border border-outline-variant flex items-center gap-3">
      <Icon size={18} className="text-primary shrink-0" />
      <div>
        <p className="font-label-caps text-[10px] text-on-surface-variant leading-none mb-1 uppercase">{label}</p>
        <p className="font-data-mono text-headline-sm leading-none text-on-surface">
          {value == null ? '—' : `${value}${unit}`}
        </p>
      </div>
    </div>
  );
}

function RiskPanel({ current }) {
  const t = useT();
  const risk = current?.risk ?? 'unknown';
  const m = riskMeta(risk);
  const inputs = current?.inputs ?? {};
  const aiOffline = current?.aiOnline === false;
  return (
    <div className="panel industrial-top p-5">
      <h3 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <BrainCircuit size={14} />
        {t('waterStress.estimateTitle')}
        <span
          className={`ml-auto inline-flex items-center gap-1.5 font-label-caps text-[9px] tracking-widest ${
            aiOffline ? 'text-error' : 'text-primary/70'
          }`}
        >
          <span className={`w-1.5 h-1.5 rounded-full ${aiOffline ? 'bg-error' : 'bg-primary'}`} />
          {aiOffline ? t('waterStress.aiOfflineLastKnown') : 'SMARTFARM-AI'}
        </span>
      </h3>

      <div className="flex flex-wrap items-center gap-4 mb-5">
        <div className={`inline-flex items-center gap-3 px-4 py-2 rounded border ${m.bg} ${m.border}`}>
          <span className={`w-3 h-3 rounded-full ${m.dot}`} />
          <span className={`font-display-lg text-[28px] leading-none ${m.text}`}>{t(m.labelKey)}</span>
        </div>
        <div className="flex flex-wrap gap-2">
          <StatChip Icon={Droplet} label={t('waterStress.soil')} value={inputs.soilMoisture} unit="%" />
          <StatChip Icon={Thermometer} label={t('waterStress.temp')} value={inputs.temperature} unit="°C" />
          <StatChip Icon={Waves} label={t('waterStress.humidity')} value={inputs.humidity} unit="%" />
        </div>
      </div>

      <div className="rounded border border-outline-variant bg-surface-container-low p-3">
        <p className="font-label-caps text-[10px] text-on-surface-variant mb-2 flex items-center gap-1.5">
          <Info size={12} /> {t('waterStress.why')}
        </p>
        <ul className="space-y-1">
          {(current?.factors ?? []).map((f, i) => (
            <li key={i} className="font-data-mono text-[11px] text-on-surface-variant leading-relaxed">
              • {f}
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
}

function TrendPanel({ history }) {
  const t = useT();
  const bandLabel = { 1: t('risk.low'), 2: t('risk.medium'), 3: t('risk.high') };
  const data = history.map((p) => ({ t: new Date(p.at).getTime(), value: bandOf(p) }));
  return (
    <div className="panel industrial-top overflow-hidden relative min-h-[300px]">
      <div className="p-5 flex flex-wrap gap-2 justify-between items-center border-b border-outline-variant/30">
        <h3 className="font-headline-sm text-headline-sm text-on-background">{t('insights.riskTrend')}</h3>
        <span className="bg-surface-container-highest px-3 py-1 text-[10px] font-data-mono text-on-surface-variant">
          {t('insights.points', { n: data.length })}
        </span>
      </div>
      <div className="h-64 w-full p-2">
        {data.length < 2 ? (
          <div className="h-full flex items-center justify-center text-on-surface-variant font-data-mono text-xs">
            {t('insights.collectingHistory')}
          </div>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            <AreaChart data={data} margin={{ top: 10, right: 16, left: 0, bottom: 0 }}>
              <defs>
                <linearGradient id="wsFill" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor={SERIES} stopOpacity={0.35} />
                  <stop offset="100%" stopColor={SERIES} stopOpacity={0} />
                </linearGradient>
              </defs>
              <CartesianGrid stroke={GRID} strokeOpacity={0.5} vertical={false} />
              <XAxis dataKey="t" tickFormatter={fmtTime} stroke={AXIS} tick={{ fontSize: 10, fontFamily: 'JetBrains Mono' }} minTickGap={40} />
              <YAxis
                stroke={AXIS}
                tick={{ fontSize: 10, fontFamily: 'JetBrains Mono' }}
                width={64}
                domain={[1, 3]}
                ticks={[1, 2, 3]}
                tickFormatter={(v) => bandLabel[v] || ''}
              />
              <Tooltip
                labelFormatter={fmtTime}
                formatter={(v) => [bandLabel[v] || '—', t('waterStress.riskTitle')]}
                contentStyle={{ background: '#1e2023', border: '1px solid #3d4a3e', fontFamily: 'JetBrains Mono', fontSize: 12, color: '#e2e2e6' }}
              />
              <Area type="stepAfter" dataKey="value" stroke={SERIES} strokeWidth={2} fill="url(#wsFill)" isAnimationActive={false} connectNulls={false} />
            </AreaChart>
          </ResponsiveContainer>
        )}
      </div>
    </div>
  );
}

function CanopyPanel({ current }) {
  const t = useT();
  const pct = current?.canopyPercent;
  const aiOffline = current?.aiOnline === false;
  const hasValue = typeof pct === 'number';
  // Cache-bust the preview so it refreshes each tick (keyed on the result time).
  const previewSrc = hasValue ? `/api/v1/canopy/preview.png?ts=${encodeURIComponent(current.at || '')}` : null;
  return (
    <div className="panel industrial-top p-5">
      <h3 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <Sprout size={14} />
        {t('insights.canopyTitle')}
        <span
          className={`ml-auto inline-flex items-center gap-1.5 font-label-caps text-[9px] tracking-widest ${
            aiOffline ? 'text-error' : 'text-primary/70'
          }`}
        >
          <span className={`w-1.5 h-1.5 rounded-full ${aiOffline ? 'bg-error' : 'bg-primary'}`} />
          {aiOffline ? t('waterStress.aiOfflineLastKnown') : 'SMARTFARM-AI'}
        </span>
      </h3>

      <div className="grid grid-cols-1 sm:grid-cols-2 gap-5 items-start">
        <div>
          <p className="font-display-lg text-[44px] leading-none text-primary">
            {hasValue ? `${pct}%` : <span className="text-on-surface-variant text-[28px]">n/a</span>}
          </p>
          <p className="font-body-md text-outline mt-2">{t('insights.canopyCover')}</p>
          <div className="mt-4 rounded border border-outline-variant bg-surface-container-low p-3">
            <ul className="space-y-1">
              {(current?.factors ?? []).map((f, i) => (
                <li key={i} className="font-data-mono text-[11px] text-on-surface-variant leading-relaxed">
                  • {f}
                </li>
              ))}
            </ul>
          </div>
        </div>
        <div className="border border-outline-variant bg-surface-container-lowest aspect-video overflow-hidden flex items-center justify-center">
          {previewSrc ? (
            <img src={previewSrc} alt="Canopy detection mask" className="w-full h-full object-contain" />
          ) : (
            <span className="font-data-mono text-[11px] text-on-surface-variant">{t('insights.canopyNoPreview')}</span>
          )}
        </div>
      </div>
    </div>
  );
}

function CanopyTrend({ history }) {
  const t = useT();
  const data = history.map((p) => ({ t: new Date(p.at).getTime(), value: p.canopyPercent }));
  return (
    <div className="panel industrial-top overflow-hidden relative min-h-[280px]">
      <div className="p-5 flex flex-wrap gap-2 justify-between items-center border-b border-outline-variant/30">
        <h3 className="font-headline-sm text-headline-sm text-on-background">{t('insights.canopyTrend')}</h3>
        <span className="bg-surface-container-highest px-3 py-1 text-[10px] font-data-mono text-on-surface-variant">
          {t('insights.points', { n: data.length })}
        </span>
      </div>
      <div className="h-56 w-full p-2">
        {data.length < 2 ? (
          <div className="h-full flex items-center justify-center text-on-surface-variant font-data-mono text-xs">
            {t('insights.collectingHistory')}
          </div>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            <AreaChart data={data} margin={{ top: 10, right: 16, left: 0, bottom: 0 }}>
              <defs>
                <linearGradient id="canopyFill" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor={SERIES} stopOpacity={0.35} />
                  <stop offset="100%" stopColor={SERIES} stopOpacity={0} />
                </linearGradient>
              </defs>
              <CartesianGrid stroke={GRID} strokeOpacity={0.5} vertical={false} />
              <XAxis dataKey="t" tickFormatter={fmtTime} stroke={AXIS} tick={{ fontSize: 10, fontFamily: 'JetBrains Mono' }} minTickGap={40} />
              <YAxis stroke={AXIS} tick={{ fontSize: 10, fontFamily: 'JetBrains Mono' }} width={40} domain={[0, 100]} tickFormatter={(v) => `${v}%`} />
              <Tooltip
                labelFormatter={fmtTime}
                formatter={(v) => [`${v}%`, t('insights.canopyTitle')]}
                contentStyle={{ background: '#1e2023', border: '1px solid #3d4a3e', fontFamily: 'JetBrains Mono', fontSize: 12, color: '#e2e2e6' }}
              />
              <Area type="monotone" dataKey="value" stroke={SERIES} strokeWidth={2} fill="url(#canopyFill)" isAnimationActive={false} connectNulls={false} />
            </AreaChart>
          </ResponsiveContainer>
        )}
      </div>
    </div>
  );
}

function DiseasePanel() {
  const t = useT();
  const { data: current } = useGetDiseaseQuery();
  const { data: hist } = useGetDiseaseHistoryQuery(15);
  const [analyze, { isLoading }] = useAnalyzeDiseaseMutation();
  const status = current?.status ?? 'idle';
  const m = diseaseMeta(status);
  const busy = isLoading || current?.analyzing;
  const history = hist?.entries ?? [];

  return (
    <div className="panel industrial-top p-5">
      <h3 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <Bug size={14} />
        {t('disease.title')}
        <span className="ml-auto font-label-caps text-[9px] text-primary/70 tracking-widest">PLANTVILLAGE · ON-DEMAND</span>
      </h3>

      <div className="flex flex-wrap items-center gap-4 mb-4">
        <div className={`inline-flex items-center gap-3 px-4 py-2 rounded border ${m.bg} ${m.border}`}>
          <span className={`w-2.5 h-2.5 rounded-full ${m.dot}`} />
          <span className={`font-headline-sm text-headline-sm ${m.text}`}>{current?.headline ?? t('disease.notAnalyzed')}</span>
        </div>
        <button
          type="button"
          onClick={() => analyze()}
          disabled={busy}
          className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110 disabled:opacity-50"
        >
          <ScanSearch size={16} />
          {busy ? t('disease.analyzing') : t('disease.analyzeFrame')}
        </button>
      </div>

      {current?.detail ? (
        <p className="mb-3 font-data-mono text-[11px] text-error/90">{t('disease.modelHint', { detail: current.detail })}</p>
      ) : null}

      {current?.top?.length ? (
        <div className="rounded border border-outline-variant bg-surface-container-low p-3 mb-4">
          <p className="font-label-caps text-[10px] text-on-surface-variant mb-2">{t('disease.topPredictions')}</p>
          <ul className="space-y-1.5">
            {current.top.map((t, i) => (
              <li key={i} className="flex items-center gap-3">
                <span className="font-data-mono text-[11px] text-on-surface flex-1 truncate">{t.label}</span>
                <span className="font-data-mono text-[11px] text-on-surface-variant tabular-nums">{t.confidence}%</span>
              </li>
            ))}
          </ul>
        </div>
      ) : null}

      {history.length ? (
        <div>
          <p className="font-label-caps text-[10px] text-on-surface-variant mb-2">{t('disease.recentChecks')}</p>
          <ul className="max-h-40 overflow-y-auto divide-y divide-outline-variant/20">
            {history.map((e) => (
              <li key={e.id} className="flex items-center gap-3 py-1.5">
                <span className={`w-1.5 h-1.5 rounded-full shrink-0 ${diseaseMeta(e.status).dot}`} />
                <span className="font-data-mono text-[11px] text-on-surface-variant flex-1 truncate">{e.headline}</span>
                <span className="font-data-mono text-[10px] text-on-surface-variant/70 shrink-0">
                  {new Date(e.at).toLocaleString(undefined, { month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' })}
                </span>
              </li>
            ))}
          </ul>
        </div>
      ) : null}
    </div>
  );
}

export default function AiInsightsPage() {
  const t = useT();
  const { data: current } = useGetWaterStressQuery(undefined, { pollingInterval: POLL_MS });
  const { data: hist } = useGetWaterStressHistoryQuery(288, { pollingInterval: 30000 });
  const history = hist?.points ?? [];
  const { data: canopy } = useGetCanopyQuery(undefined, { pollingInterval: POLL_MS });
  const { data: canopyHist } = useGetCanopyHistoryQuery(288, { pollingInterval: 30000 });
  const canopyHistory = canopyHist?.points ?? [];

  return (
    <div className="grid grid-cols-1 md:grid-cols-12 gap-gutter">
      <section className="md:col-span-12 mb-4">
        <div className="flex items-center gap-2 mb-1">
          <span className="flex items-center gap-2 bg-primary/10 border border-primary/40 text-primary px-3 py-1 font-data-mono text-[12px] uppercase tracking-widest">
            <BrainCircuit size={13} />
            {t('nav.insights')}
          </span>
        </div>
        <h2 className="font-display-lg text-display-lg text-on-background">{t('insights.pageTitle')}</h2>
        <p className="font-body-md text-on-surface-variant text-sm mt-1">{t('insights.pageIntro')}</p>
      </section>

      <div className="md:col-span-12">
        <RiskPanel current={current} />
      </div>
      <div className="md:col-span-12">
        <TrendPanel history={history} />
      </div>

      <div className="md:col-span-12">
        <CanopyPanel current={canopy} />
      </div>
      <div className="md:col-span-12">
        <CanopyTrend history={canopyHistory} />
      </div>

      <div className="md:col-span-12">
        <DiseasePanel />
      </div>
    </div>
  );
}
