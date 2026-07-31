import { useMemo, useState, useEffect } from 'react';
import { useSelector } from 'react-redux';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { selectHistory } from '../history/historySlice';
import { metricMeta } from '../../lib/metricMeta';
import { useT } from '../../i18n';
import { useGetSettingsQuery, useUpdateSettingsMutation } from '../settings/settingsApi';
import { useGetTelemetryHistoryQuery } from './devicesApi';

const AXIS = '#bbcbbb'; // on-surface-variant
const GRID = '#3d4a3e'; // outline-variant

// 3 distinct high-contrast series colors matching the design system
const SLOT_COLORS = [
  '#92ccff', // Tech Blue (Slot 1)
  '#54e98a', // Primary Mint (Slot 2)
  '#f7c919', // Tertiary Amber (Slot 3)
];

const LOCAL_STORAGE_KEY = 'smartfarm_trend_indicators';

function fmtTime(t, range = 'current') {
  const d = new Date(t);
  const hours = String(d.getHours()).padStart(2, '0');
  const minutes = String(d.getMinutes()).padStart(2, '0');
  if (range === 'day') {
    const month = String(d.getMonth() + 1).padStart(2, '0');
    const day = String(d.getDate()).padStart(2, '0');
    return `${month}/${day} ${hours}:${minutes}`;
  }
  if (range === 'hour') {
    return `${hours}:${minutes}`;
  }
  const seconds = String(d.getSeconds()).padStart(2, '0');
  return `${hours}:${minutes}:${seconds}`;
}

function metaLabel(t, meta) {
  return meta.labelKey ? t(meta.labelKey) : meta.label;
}

function CustomTooltip({ active, payload, label, activeSlots, t, range }) {
  if (!active || !payload || !payload.length) return null;
  return (
    <div className="bg-surface-container border border-outline-variant p-3 font-data-mono text-xs shadow-xl rounded z-50">
      <div className="text-on-surface-variant mb-2 pb-1 border-b border-outline-variant/30 font-bold">
        {fmtTime(label, range)}
      </div>
      <div className="space-y-1.5">
        {payload.map((entry) => {
          const slotIdx = Number(String(entry.dataKey).replace('v_', ''));
          const slot = activeSlots.find((s) => s.index === slotIdx);
          if (!slot) return null;
          const meta = metricMeta(slot.metric);
          const name = metaLabel(t, meta);
          return (
            <div key={entry.dataKey} className="flex items-center gap-2 min-w-[200px]">
              <span className="w-2.5 h-2.5 rounded-full shrink-0" style={{ backgroundColor: slot.color }} />
              <span className="text-on-surface-variant truncate">
                {slot.deviceId} · {name}:
              </span>
              <span className="text-on-surface font-bold ml-auto pl-2 whitespace-nowrap">
                {entry.value} {meta.unit || ''}
              </span>
            </div>
          );
        })}
      </div>
    </div>
  );
}

export default function TrendChart() {
  const t = useT();
  const livePoints = useSelector(selectHistory);
  const { data: appSettings } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();

  const [timeRange, setTimeRange] = useState('current'); // 'current', 'hour', 'day'

  // Fetch historical telemetry from backend when timeRange is 'hour' or 'day'
  const { data: historyPoints = [] } = useGetTelemetryHistoryQuery(timeRange, {
    skip: timeRange === 'current',
    pollingInterval: 30000,
  });

  const points = timeRange === 'current' ? livePoints : historyPoints;

  const seriesKeys = useMemo(() => {
    const keys = new Set();
    points.forEach((p) => Object.keys(p.values || {}).forEach((k) => keys.add(k)));
    return Array.from(keys).sort();
  }, [points]);

  const seriesLabel = (key) => {
    if (!key) return t('trend.none');
    const [deviceId, metric] = key.split('::');
    return `${deviceId} · ${metaLabel(t, metricMeta(metric))}`;
  };

  // 3 slot indicators state
  const [selectedIndicators, setSelectedIndicators] = useState(() => {
    try {
      const saved = localStorage.getItem(LOCAL_STORAGE_KEY);
      if (saved) {
        const parsed = JSON.parse(saved);
        if (Array.isArray(parsed) && parsed.length === 3) return parsed;
      }
    } catch {
      // ignore
    }
    return ['', '', ''];
  });

  // Sync server settings into state when loaded
  useEffect(() => {
    const serverInds = appSettings?.trend?.indicators;
    if (Array.isArray(serverInds) && serverInds.length === 3) {
      if (serverInds.some((s) => s !== '')) {
        setSelectedIndicators(serverInds);
        localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(serverInds));
      }
    }
  }, [appSettings]);

  // Seed default selections if seriesKeys are populated and slots are blank
  useEffect(() => {
    if (seriesKeys.length > 0 && selectedIndicators.every((s) => s === '')) {
      const initial = [
        seriesKeys[0] || '',
        seriesKeys[1] || '',
        seriesKeys[2] || '',
      ];
      setSelectedIndicators(initial);
      localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(initial));
      updateSettings({ trend: { indicators: initial } });
    }
  }, [seriesKeys, selectedIndicators, updateSettings]);

  const handleSelectSlot = (index, value) => {
    const next = [...selectedIndicators];
    next[index] = value;
    setSelectedIndicators(next);
    localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(next));
    updateSettings({ trend: { indicators: next } });
  };

  // Determine active slots
  const activeSlots = useMemo(() => {
    return selectedIndicators
      .map((key, index) => ({
        index,
        key,
        color: SLOT_COLORS[index],
        metric: key ? key.split('::')[1] : '',
        deviceId: key ? key.split('::')[0] : '',
      }))
      .filter((s) => s.key);
  }, [selectedIndicators]);

  // Build aggregated data array for Recharts
  const data = useMemo(() => {
    if (activeSlots.length === 0 || points.length === 0) return [];
    return points
      .filter((p) => activeSlots.some((slot) => slot.key in (p.values || {})))
      .map((p) => {
        const entry = { t: p.t };
        activeSlots.forEach((slot) => {
          if (slot.key in (p.values || {})) {
            entry[`v_${slot.index}`] = p.values[slot.key];
          }
        });
        return entry;
      });
  }, [points, activeSlots]);

  return (
    <div className="panel industrial-top overflow-hidden relative min-h-[320px]">
      <div className="p-5 flex flex-wrap gap-3 justify-between items-center border-b border-outline-variant/30">
        <div className="flex flex-wrap items-center gap-3">
          <h3 className="font-headline-sm text-headline-sm text-on-background">{t('trend.title')}</h3>

          {/* Segmented Time Range Selector */}
          <div className="flex items-center bg-surface-container-highest p-0.5 border border-outline-variant rounded">
            {['current', 'hour', 'day'].map((range) => (
              <button
                key={range}
                type="button"
                onClick={() => setTimeRange(range)}
                className={`px-2.5 py-1 text-xs font-data-mono transition-colors rounded ${
                  timeRange === range
                    ? 'bg-primary text-on-primary font-bold shadow'
                    : 'text-on-surface-variant hover:text-on-surface'
                }`}
              >
                {t(`trend.range${range.charAt(0).toUpperCase() + range.slice(1)}`)}
              </button>
            ))}
          </div>
        </div>

        <div className="flex flex-wrap items-center gap-2">
          {[0, 1, 2].map((slotIdx) => {
            const slotColor = SLOT_COLORS[slotIdx];
            const slotKey = selectedIndicators[slotIdx] || '';
            const labelKey = `slot${slotIdx + 1}`;
            return (
              <div key={slotIdx} className="flex items-center gap-1.5 bg-surface-container-highest px-2 py-1 border border-outline-variant">
                <span className="w-2.5 h-2.5 rounded-full shrink-0" style={{ backgroundColor: slotColor }} />
                <select
                  value={slotKey}
                  onChange={(e) => handleSelectSlot(slotIdx, e.target.value)}
                  className="bg-transparent text-on-surface font-data-mono text-xs focus:outline-none cursor-pointer max-w-[160px] sm:max-w-[200px]"
                  title={t(`trend.${labelKey}`)}
                  aria-label={t(`trend.${labelKey}`)}
                >
                  <option value="" className="bg-surface-container-highest text-on-surface-variant">
                    {t('trend.none')}
                  </option>
                  {seriesKeys.map((k) => (
                    <option key={k} value={k} className="bg-surface-container-highest text-on-surface">
                      {seriesLabel(k)}
                    </option>
                  ))}
                </select>
              </div>
            );
          })}

          <span className="bg-surface-container-highest px-3 py-1 text-[12px] font-data-mono text-on-surface-variant border border-outline-variant/50">
            {t('trend.samples', { n: data.length })}
          </span>
        </div>
      </div>

      <div className="h-64 w-full p-2">
        {data.length < 2 || activeSlots.length === 0 ? (
          <div className="h-full flex items-center justify-center text-on-surface-variant font-data-mono text-xs">
            {seriesKeys.length === 0 && points.length === 0
              ? t('trend.waitingNumeric')
              : activeSlots.length === 0
              ? t('trend.none')
              : t('trend.collecting')}
          </div>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={data} margin={{ top: 10, right: 16, left: 0, bottom: 0 }}>
              <CartesianGrid stroke={GRID} strokeOpacity={0.5} vertical={false} />
              <XAxis
                dataKey="t"
                tickFormatter={(tVal) => fmtTime(tVal, timeRange)}
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
              <Tooltip content={<CustomTooltip activeSlots={activeSlots} t={t} range={timeRange} />} />
              {activeSlots.map((slot) => (
                <Line
                  key={slot.index}
                  type="monotone"
                  dataKey={`v_${slot.index}`}
                  stroke={slot.color}
                  strokeWidth={2.5}
                  dot={false}
                  activeDot={{ r: 5, fill: slot.color, stroke: '#111316', strokeWidth: 2 }}
                  isAnimationActive={false}
                />
              ))}
            </LineChart>
          </ResponsiveContainer>
        )}
      </div>
    </div>
  );
}
