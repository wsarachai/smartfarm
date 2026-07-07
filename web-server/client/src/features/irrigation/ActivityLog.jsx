import { ScrollText, Play, Square, SkipForward, TimerReset, AlertTriangle } from 'lucide-react';
import { useGetIrrigationLogQuery } from './irrigationApi';
import { useT } from '../../i18n';

const LOG_POLL_MS = 5000;

// Map a log entry to an icon, color, and human-readable line. `e.error`/`e.note`
// come from the backend (English, out of scope); the templates around them are
// translated.
function describe(e, t) {
  if (!e.ok) {
    const action = e.action === 'on' ? t('activity.start') : e.action === 'off' ? t('activity.stop') : e.action;
    const base = t('activity.failed', { action, error: e.error || 'error' });
    return { Icon: AlertTriangle, color: 'text-error', text: `${base}${e.label ? ` (${e.label})` : ''}` };
  }
  if (e.action === 'skip') {
    const reason = e.note || (e.moisture != null ? t('activity.soilWet', { moisture: e.moisture }) : t('activity.moistureGuard'));
    return {
      Icon: SkipForward,
      color: 'text-tertiary',
      text: t('activity.skipped', { label: e.label || t('activity.scheduledRun'), reason }),
    };
  }
  if (e.action === 'on') {
    return {
      Icon: Play,
      color: 'text-primary',
      text:
        e.source === 'schedule'
          ? e.durationMinutes
            ? t('activity.ranFor', { label: e.label || t('activity.scheduledRun'), min: e.durationMinutes })
            : t('activity.ran', { label: e.label || t('activity.scheduledRun') })
          : e.durationMinutes
            ? t('activity.manualOnAutoOff', { min: e.durationMinutes })
            : t('activity.manualOn'),
    };
  }
  // action === 'off'
  if (e.source === 'auto-off') {
    return {
      Icon: TimerReset,
      color: 'text-on-surface-variant',
      text: e.label ? t('activity.autoOffEnded', { label: e.label }) : t('activity.autoOffSafety'),
    };
  }
  return { Icon: Square, color: 'text-on-surface-variant', text: t('activity.manualOff') };
}

function formatWhen(iso) {
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return String(iso);
  return d.toLocaleString(undefined, {
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

export default function ActivityLog() {
  const t = useT();
  const { data } = useGetIrrigationLogQuery(50, { pollingInterval: LOG_POLL_MS });
  const entries = data?.entries ?? [];

  return (
    <div className="bg-surface-container p-5 border border-outline-variant">
      <h4 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <ScrollText size={14} />
        {t('activity.title')}
        <span className="ml-auto font-data-mono text-[10px] text-on-surface-variant/60">
          {t('activity.recent', { n: entries.length })}
        </span>
      </h4>

      {entries.length === 0 ? (
        <p className="font-data-mono text-[11px] text-on-surface-variant py-2">
          {t('activity.none')}
        </p>
      ) : (
        <ul className="max-h-80 overflow-y-auto divide-y divide-outline-variant/20">
          {entries.map((e) => {
            const { Icon, color, text } = describe(e, t);
            return (
              <li key={e.id} className="flex items-start gap-3 py-2.5">
                <Icon size={15} className={`mt-0.5 shrink-0 ${color}`} />
                <div className="min-w-0 flex-1">
                  <p className={`font-data-mono text-[12px] leading-snug ${color}`}>{text}</p>
                  <p className="font-data-mono text-[10px] text-on-surface-variant/70 mt-0.5">
                    {formatWhen(e.at)}
                  </p>
                </div>
              </li>
            );
          })}
        </ul>
      )}
    </div>
  );
}
