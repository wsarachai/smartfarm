import { Radio } from 'lucide-react';
import Led from '../../components/Led';
import { useT } from '../../i18n';
import { freshness } from '../../lib/freshness';
import { metricMeta, formatMetricValue } from '../../lib/metricMeta';

// One telemetry card per sensor device (the wireframe's "Zone" card, driven by
// real data). Title is the device_id; each metric renders via the metadata map.
export default function SensorCard({ device }) {
  const t = useT();
  const status = freshness(device.lastSeen);
  const rows = Object.entries(device.metrics || {});

  return (
    <div className="panel industrial-top p-5 hover:bg-surface-container-high transition-colors">
      <div className="flex justify-between items-start mb-6">
        <div>
          <span className="font-label-caps text-label-caps text-on-surface-variant">{t('sensor.tag')}</span>
          <h3 className="font-headline-md text-headline-md text-primary break-all">{device.device_id}</h3>
        </div>
        <div className="flex items-center gap-2">
          <Led status={status} />
          <Radio size={20} className="text-on-surface-variant" />
        </div>
      </div>

      <div className="space-y-4">
        {rows.length === 0 && (
          <p className="font-body-md text-on-surface-variant">{t('sensor.noMetrics')}</p>
        )}
        {rows.map(([key, value], i) => {
          const { label, labelKey, unit, Icon } = metricMeta(key);
          const last = i === rows.length - 1;
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
  );
}
