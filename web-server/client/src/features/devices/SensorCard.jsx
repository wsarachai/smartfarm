import { useState } from 'react';
import { Radio, ArrowUpDown, ArrowUp, ArrowDown } from 'lucide-react';
import Led from '../../components/Led';
import WidgetCard from '../../components/WidgetCard';
import { useT } from '../../i18n';
import { freshness } from '../../lib/freshness';
import { metricMeta, formatMetricValue } from '../../lib/metricMeta';

// One telemetry card per sensor device (the wireframe's "Zone" card, driven by
// real data). Title is the device_id; each metric renders via the metadata map.
export default function SensorCard({ device }) {
  const t = useT();
  const [sortMode, setSortMode] = useState('none'); // 'none' | 'asc' | 'desc'
  const status = freshness(device.lastSeen);
  const rows = Object.entries(device.metrics || {});

  const cycleSort = () => {
    setSortMode((prev) => (prev === 'none' ? 'asc' : prev === 'asc' ? 'desc' : 'none'));
  };

  const sortedRows = [...rows].sort(([, valA], [, valB]) => {
    if (sortMode === 'none') return 0;
    const numA = typeof valA === 'number' ? valA : Number(valA);
    const numB = typeof valB === 'number' ? valB : Number(valB);
    const isNumA = !isNaN(numA);
    const isNumB = !isNaN(numB);

    if (isNumA && isNumB) {
      return sortMode === 'asc' ? numA - numB : numB - numA;
    }
    if (isNumA) return -1;
    if (isNumB) return 1;
    return 0;
  });

  const sortTitle =
    sortMode === 'asc'
      ? t('common.sortAsc')
      : sortMode === 'desc'
      ? t('common.sortDesc')
      : t('common.sortDefault');

  return (
    <WidgetCard className="industrial-top hover:bg-surface-container-high transition-colors">
      <div className="p-5">
        <div className="flex justify-between items-start mb-6">
          <div>
            <span className="font-label-caps text-label-caps text-on-surface-variant">{t('sensor.tag')}</span>
            <h3 className="font-headline-md text-headline-md text-primary break-all">{device.device_id}</h3>
          </div>
          <div className="flex items-center gap-2">
            {rows.length > 1 && (
              <button
                type="button"
                onClick={cycleSort}
                title={sortTitle}
                aria-label={sortTitle}
                className={`p-1 rounded transition-colors ${
                  sortMode !== 'none'
                    ? 'text-primary bg-primary/10 border border-primary/30'
                    : 'text-on-surface-variant/70 hover:text-on-surface hover:bg-surface-container-highest'
                }`}
              >
                {sortMode === 'asc' ? (
                  <ArrowUp size={16} />
                ) : sortMode === 'desc' ? (
                  <ArrowDown size={16} />
                ) : (
                  <ArrowUpDown size={16} />
                )}
              </button>
            )}
            <Led status={status} />
            <Radio size={20} className="text-on-surface-variant" />
          </div>
        </div>

        <div className="space-y-4">
          {sortedRows.length === 0 && (
            <p className="font-body-md text-on-surface-variant">{t('sensor.noMetrics')}</p>
          )}
          {sortedRows.map(([key, value], i) => {
            const { label, labelKey, unit, Icon } = metricMeta(key);
            const last = i === sortedRows.length - 1;
            return (
              <div
                key={key}
                className={`flex items-center justify-between ${last ? '' : 'border-b border-outline-variant/30 pb-3'}`}
              >
                <div className="flex items-center gap-3 min-w-0">
                  {Icon ? (
                    <Icon size={20} className="text-secondary shrink-0" />
                  ) : (
                    <span className="w-5 shrink-0" />
                  )}
                  <span className="font-body-md text-on-surface truncate">{labelKey ? t(labelKey) : label}</span>
                </div>
                <span className="font-data-mono text-headline-sm text-on-background whitespace-nowrap">
                  {formatMetricValue(value)}
                  {unit ? ` ${unit}` : ''}
                </span>
              </div>
            );
          })}
        </div>
      </div>
    </WidgetCard>
  );
}
