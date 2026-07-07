import { useMemo, useState } from 'react';
import { useSelector } from 'react-redux';
import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { selectHistory } from '../history/historySlice';
import { metricMeta } from '../../lib/metricMeta';
import { useT } from '../../i18n';

const AXIS = '#bbcbbb'; // on-surface-variant
const GRID = '#3d4a3e'; // outline-variant
const SERIES = '#92ccff'; // secondary (Tech Blue — reserved for data viz)

function fmtTime(t) {
  const d = new Date(t);
  return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}:${String(
    d.getSeconds()
  ).padStart(2, '0')}`;
}

// Translate a metric-meta label (known keys carry a labelKey; unknown fall back
// to the humanized English label).
function metaLabel(t, meta) {
  return meta.labelKey ? t(meta.labelKey) : meta.label;
}

export default function TrendChart() {
  const t = useT();
  const points = useSelector(selectHistory);
  const [selected, setSelected] = useState(null);
  const seriesLabel = (key) => {
    const [deviceId, metric] = key.split('::');
    return `${deviceId} · ${metaLabel(t, metricMeta(metric))}`;
  };

  const seriesKeys = useMemo(() => {
    const keys = new Set();
    points.forEach((p) => Object.keys(p.values).forEach((k) => keys.add(k)));
    return Array.from(keys).sort();
  }, [points]);

  const active = selected && seriesKeys.includes(selected) ? selected : seriesKeys[0];

  const data = useMemo(
    () =>
      active
        ? points
            .filter((p) => active in p.values)
            .map((p) => ({ t: p.t, value: p.values[active] }))
        : [],
    [points, active]
  );

  const metric = active ? active.split('::')[1] : '';
  const unit = active ? metricMeta(metric).unit : '';

  return (
    <div className="panel industrial-top overflow-hidden relative min-h-[300px]">
      <div className="p-5 flex flex-wrap gap-2 justify-between items-center border-b border-outline-variant/30">
        <h3 className="font-headline-sm text-headline-sm text-on-background">{t('trend.title')}</h3>
        <div className="flex items-center gap-2">
          {seriesKeys.length > 0 && (
            <select
              value={active}
              onChange={(e) => setSelected(e.target.value)}
              className="bg-surface-container-highest text-on-surface font-data-mono text-xs px-3 py-1 border border-outline-variant focus:border-secondary focus:outline-none"
            >
              {seriesKeys.map((k) => (
                <option key={k} value={k}>
                  {seriesLabel(k)}
                </option>
              ))}
            </select>
          )}
          <span className="bg-surface-container-highest px-3 py-1 text-[10px] font-data-mono text-on-surface-variant">
            {t('trend.samples', { n: data.length })}
          </span>
        </div>
      </div>

      <div className="h-64 w-full p-2">
        {data.length < 2 ? (
          <div className="h-full flex items-center justify-center text-on-surface-variant font-data-mono text-xs">
            {seriesKeys.length === 0 ? t('trend.waitingNumeric') : t('trend.collecting')}
          </div>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            <AreaChart data={data} margin={{ top: 10, right: 16, left: 0, bottom: 0 }}>
              <defs>
                <linearGradient id="trendFill" x1="0" y1="0" x2="0" y2="1">
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
                formatter={(v) => [`${v}${unit ? ` ${unit}` : ''}`, metaLabel(t, metricMeta(metric))]}
                contentStyle={{
                  background: '#1e2023',
                  border: '1px solid #3d4a3e',
                  fontFamily: 'JetBrains Mono',
                  fontSize: 12,
                  color: '#e2e2e6',
                }}
              />
              <Area type="monotone" dataKey="value" stroke={SERIES} strokeWidth={2} fill="url(#trendFill)" isAnimationActive={false} />
            </AreaChart>
          </ResponsiveContainer>
        )}
      </div>
    </div>
  );
}
