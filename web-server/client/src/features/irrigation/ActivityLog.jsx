import { ScrollText, Play, Square, SkipForward, TimerReset, AlertTriangle } from 'lucide-react';
import { useGetIrrigationLogQuery } from './irrigationApi';

const LOG_POLL_MS = 5000;

// Map a log entry to an icon, color, and human-readable line.
function describe(e) {
  if (!e.ok) {
    return {
      Icon: AlertTriangle,
      color: 'text-error',
      text: `${e.action === 'on' ? 'Start' : e.action === 'off' ? 'Stop' : e.action} failed — ${e.error || 'error'}${e.label ? ` (${e.label})` : ''}`,
    };
  }
  if (e.action === 'skip') {
    return {
      Icon: SkipForward,
      color: 'text-tertiary',
      text: `${e.label || 'Scheduled run'} skipped — ${e.note || (e.moisture != null ? `soil ${e.moisture}% (wet)` : 'moisture guard')}`,
    };
  }
  if (e.action === 'on') {
    return {
      Icon: Play,
      color: 'text-primary',
      text:
        e.source === 'schedule'
          ? `${e.label || 'Scheduled run'} — ran${e.durationMinutes ? ` ${e.durationMinutes} min` : ''}`
          : `Manual ON${e.durationMinutes ? ` · auto-off ${e.durationMinutes}m` : ''}`,
    };
  }
  // action === 'off'
  if (e.source === 'auto-off') {
    return {
      Icon: TimerReset,
      color: 'text-on-surface-variant',
      text: e.label ? `Auto-off — ${e.label} ended` : 'Auto-off (safety window)',
    };
  }
  return { Icon: Square, color: 'text-on-surface-variant', text: 'Manual OFF' };
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
  const { data } = useGetIrrigationLogQuery(50, { pollingInterval: LOG_POLL_MS });
  const entries = data?.entries ?? [];

  return (
    <div className="bg-surface-container p-5 border border-outline-variant">
      <h4 className="font-label-caps text-label-caps text-on-surface-variant mb-4 flex items-center gap-2">
        <ScrollText size={14} />
        Activity Log
        <span className="ml-auto font-data-mono text-[10px] text-on-surface-variant/60">
          {entries.length} recent
        </span>
      </h4>

      {entries.length === 0 ? (
        <p className="font-data-mono text-[11px] text-on-surface-variant py-2">
          No pump activity recorded yet.
        </p>
      ) : (
        <ul className="max-h-80 overflow-y-auto divide-y divide-outline-variant/20">
          {entries.map((e) => {
            const { Icon, color, text } = describe(e);
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
