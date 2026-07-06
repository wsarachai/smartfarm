import { Link } from 'react-router-dom';
import { Droplet, Thermometer, Waves, ArrowRight } from 'lucide-react';
import { useGetWaterStressQuery } from './waterStressApi';
import { riskMeta } from './risk';

const POLL_MS = 5000;

function Metric({ Icon, label, value, unit }) {
  return (
    <div className="flex items-center gap-2">
      <Icon size={14} className="text-on-surface-variant shrink-0" />
      <span className="font-data-mono text-[11px] text-on-surface-variant">{label}</span>
      <span className="font-data-mono text-[11px] text-on-surface ml-auto">
        {value == null ? 'n/a' : `${value}${unit}`}
      </span>
    </div>
  );
}

// Real, rule-based water-stress advisory (replaces the fabricated AI insight).
export default function WaterStressCard() {
  const { data } = useGetWaterStressQuery(undefined, { pollingInterval: POLL_MS });
  const risk = data?.risk ?? 'unknown';
  const m = riskMeta(risk);
  const inputs = data?.inputs ?? {};
  const why = data?.factors?.[data.factors.length - 1] || 'Estimating…';
  const aiOffline = data?.aiOnline === false;

  return (
    <div className="panel p-6 flex flex-col justify-between min-h-[220px]">
      <div>
        <div className="flex items-center justify-between mb-4">
          <span className="font-label-caps text-label-caps text-on-surface-variant">Water Stress Risk</span>
          <span
            className={`inline-flex items-center gap-1.5 font-label-caps text-label-caps ${
              aiOffline ? 'text-error' : 'text-primary/70'
            }`}
          >
            <span className={`w-1.5 h-1.5 rounded-full ${aiOffline ? 'bg-error' : 'bg-primary'}`} />
            {aiOffline ? 'AI OFFLINE' : 'SMARTFARM-AI'}
          </span>
        </div>

        <div className={`inline-flex items-center gap-2 px-3 py-1.5 rounded border ${m.bg} ${m.border} mb-4`}>
          <span className={`w-2.5 h-2.5 rounded-full ${m.dot}`} />
          <span className={`font-headline-md text-headline-md ${m.text}`}>{m.label}</span>
        </div>

        <div className="space-y-1.5 mb-4">
          <Metric Icon={Droplet} label="Soil" value={inputs.soilMoisture} unit="%" />
          <Metric Icon={Thermometer} label="Temp" value={inputs.temperature} unit="°C" />
          <Metric Icon={Waves} label="Humidity" value={inputs.humidity} unit="%" />
        </div>

        <p className="font-data-mono text-[11px] text-on-surface-variant leading-relaxed">{why}</p>
        {aiOffline ? (
          <p className="mt-2 font-data-mono text-[10px] text-error/90 leading-relaxed">
            AI service unreachable — showing last known estimate.
          </p>
        ) : null}
      </div>

      <Link
        to="/insights"
        title="Open AI Insights"
        className="mt-4 inline-flex items-center gap-2 bg-surface-container-high border border-outline-variant px-3 py-2 hover:bg-surface-container-highest transition-colors active:scale-95"
      >
        <span className="font-data-mono text-xs font-bold text-on-surface">AI INSIGHTS</span>
        <ArrowRight size={15} className="text-on-surface-variant" />
      </Link>
    </div>
  );
}
