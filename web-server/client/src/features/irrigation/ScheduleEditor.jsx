import { useEffect, useState } from 'react';
import { Clock, Plus, Trash2, Save } from 'lucide-react';
import { useGetSettingsQuery, useUpdateSettingsMutation } from '../settings/settingsApi';
import { useGetIrrigationStatusQuery } from './irrigationApi';
import { TIMEZONE_OPTIONS, matchTimezone } from './timezones';
import { useT } from '../../i18n';

const STATUS_POLL_MS = 5000;
const DUR_MIN = 1;
const DUR_MAX = 60;

function newEntry() {
  return {
    id: `new-${Date.now()}-${Math.random().toString(36).slice(2, 6)}`,
    label: '',
    start: '05:00',
    durationMinutes: 5,
    days: [0, 1, 2, 3, 4, 5, 6],
    enabled: true,
  };
}

function formatIn(minutes, t) {
  if (minutes == null) return '—';
  if (minutes <= 0) return t('schedule.now');
  const h = Math.floor(minutes / 60);
  const m = minutes % 60;
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}

export default function ScheduleEditor() {
  const t = useT();
  const DAYS = [0, 1, 2, 3, 4, 5, 6].map((i) => t(`schedule.day${i}`));
  const { data: settings } = useGetSettingsQuery();
  const [updateSettings, { isLoading: saving }] = useUpdateSettingsMutation();
  const { data: status } = useGetIrrigationStatusQuery(undefined, { pollingInterval: STATUS_POLL_MS });

  const [entries, setEntries] = useState(null);
  const [threshold, setThreshold] = useState(60);
  const [timezone, setTimezone] = useState('Asia/Bangkok');
  const [err, setErr] = useState('');
  const [saved, setSaved] = useState(false);

  // Seed the editable copy once the server settings arrive.
  useEffect(() => {
    if (settings?.irrigation && entries === null) {
      setEntries(settings.irrigation.entries.map((e) => ({ ...e, days: [...e.days] })));
      setThreshold(settings.irrigation.moistureThreshold);
      setTimezone(settings.irrigation.timezone);
    }
  }, [settings, entries]);

  const patchEntry = (id, patch) =>
    setEntries((prev) => prev.map((e) => (e.id === id ? { ...e, ...patch } : e)));

  const toggleDay = (id, day) =>
    setEntries((prev) =>
      prev.map((e) => {
        if (e.id !== id) return e;
        const has = e.days.includes(day);
        const days = has ? e.days.filter((d) => d !== day) : [...e.days, day].sort((a, b) => a - b);
        return { ...e, days };
      })
    );

  const addEntry = () => setEntries((prev) => [...prev, newEntry()]);
  const removeEntry = (id) => setEntries((prev) => prev.filter((e) => e.id !== id));

  const onSave = async () => {
    setErr('');
    try {
      const next = await updateSettings({
        irrigation: {
          moistureThreshold: Number(threshold),
          timezone: timezone.trim(),
          entries: entries.map((e) => ({
            id: e.id.startsWith('new-') ? undefined : e.id,
            label: e.label,
            start: e.start,
            durationMinutes: Number(e.durationMinutes),
            days: e.days,
            enabled: e.enabled,
          })),
        },
      }).unwrap();
      // Re-seed from the canonical server copy (server assigns ids to new rows).
      setEntries(next.irrigation.entries.map((x) => ({ ...x, days: [...x.days] })));
      setThreshold(next.irrigation.moistureThreshold);
      setTimezone(next.irrigation.timezone);
      setSaved(true);
      window.setTimeout(() => setSaved(false), 2000);
    } catch (e) {
      setErr(e?.data?.error || t('schedule.saveFailed'));
    }
  };

  if (entries === null) {
    return (
      <div className="bg-surface-container p-5 border border-outline-variant">
        <p className="font-data-mono text-xs text-on-surface-variant">{t('schedule.loading')}</p>
      </div>
    );
  }

  const auto = Boolean(settings?.irrigation?.auto);
  // The stored value is an IANA zone; find which dropdown option represents it
  // (exact, else a zone whose group contains it). null => show a "Current:" row.
  const tzMatched = matchTimezone(timezone);

  return (
    <div className="bg-surface-container p-5 border border-outline-variant">
      <div className="flex items-center justify-between mb-4 flex-wrap gap-2">
        <h4 className="font-label-caps text-label-caps text-on-surface-variant flex items-center gap-2">
          <Clock size={14} />
          {t('schedule.title')}
        </h4>
        <span
          className={`px-2 py-0.5 font-data-mono text-[9px] rounded ${
            auto ? 'bg-primary/20 text-primary' : 'bg-surface-container-high text-on-surface-variant/70'
          }`}
        >
          {auto ? t('schedule.autoActive') : t('schedule.autoOff')}
        </span>
      </div>

      {/* Scheduler status readout */}
      <div className="mb-4 rounded border border-outline-variant bg-surface-container-low p-3 space-y-1">
        <p className="font-data-mono text-[11px] text-on-surface-variant">
          {t('schedule.nextRun')}{' '}
          <span className="text-on-surface">
            {status?.nextRun
              ? t('schedule.nextRunValue', {
                  label: status.nextRun.label || status.nextRun.start,
                  start: status.nextRun.start,
                  in: formatIn(status.nextRun.inMinutes, t),
                })
              : auto
                ? t('schedule.noEnabled')
                : t('schedule.autoModeOff')}
          </span>
        </p>
        {status?.lastSkip ? (
          <p className="font-data-mono text-[11px] text-tertiary">{t('schedule.lastSkip', { reason: status.lastSkip.reason })}</p>
        ) : null}
        {status?.lastRun ? (
          <p className="font-data-mono text-[11px] text-on-surface-variant">
            {t('schedule.lastRun', { label: status.lastRun.label || status.lastRun.entryId, min: status.lastRun.durationMinutes })}
            {status.lastRun.ok ? '' : ` — ${t('schedule.failed')}`}
          </p>
        ) : null}
      </div>

      {/* Entries */}
      <div className="space-y-3">
        {entries.length === 0 ? (
          <p className="font-data-mono text-[11px] text-on-surface-variant py-2">
            {t('schedule.noEntries')}
          </p>
        ) : (
          entries.map((e) => (
            <div key={e.id} className="rounded border border-outline-variant bg-surface-container-low p-3 space-y-3">
              <div className="flex items-center gap-2 flex-wrap">
                <input
                  type="time"
                  value={e.start}
                  onChange={(ev) => patchEntry(e.id, { start: ev.target.value })}
                  className="bg-surface-container-lowest border border-outline-variant rounded px-2 py-1 font-data-mono text-sm text-on-surface"
                />
                <div className="flex items-center gap-1">
                  <input
                    type="number"
                    min={DUR_MIN}
                    max={DUR_MAX}
                    value={e.durationMinutes}
                    onChange={(ev) => patchEntry(e.id, { durationMinutes: ev.target.value })}
                    className="w-16 bg-surface-container-lowest border border-outline-variant rounded px-2 py-1 font-data-mono text-sm text-on-surface"
                  />
                  <span className="font-data-mono text-[11px] text-on-surface-variant">{t('schedule.min')}</span>
                </div>
                <input
                  type="text"
                  placeholder={t('schedule.labelPlaceholder')}
                  value={e.label}
                  onChange={(ev) => patchEntry(e.id, { label: ev.target.value })}
                  className="flex-1 min-w-[8rem] bg-surface-container-lowest border border-outline-variant rounded px-2 py-1 font-data-mono text-sm text-on-surface"
                />
                <label className="flex items-center gap-1.5 font-data-mono text-[11px] text-on-surface-variant">
                  <input
                    type="checkbox"
                    className="h-3.5 w-3.5 accent-primary"
                    checked={e.enabled}
                    onChange={(ev) => patchEntry(e.id, { enabled: ev.target.checked })}
                  />
                  {t('schedule.on')}
                </label>
                <button
                  type="button"
                  onClick={() => removeEntry(e.id)}
                  className="p-1.5 rounded text-on-surface-variant hover:text-error hover:bg-error/10"
                  aria-label={t('schedule.deleteEntry')}
                >
                  <Trash2 size={15} />
                </button>
              </div>
              <div className="flex gap-1 flex-wrap">
                {DAYS.map((d, i) => {
                  const active = e.days.includes(i);
                  return (
                    <button
                      key={d}
                      type="button"
                      onClick={() => toggleDay(e.id, i)}
                      className={`px-2 py-1 rounded font-data-mono text-[10px] border transition-colors ${
                        active
                          ? 'bg-primary text-on-primary border-primary'
                          : 'bg-surface-container-lowest text-on-surface-variant border-outline-variant hover:border-primary/50'
                      }`}
                    >
                      {d}
                    </button>
                  );
                })}
              </div>
            </div>
          ))
        )}
      </div>

      <button
        type="button"
        onClick={addEntry}
        className="mt-3 inline-flex items-center gap-2 bg-surface-container-high border border-outline-variant text-on-surface px-3 py-2 rounded font-label-caps text-label-caps hover:bg-surface-container-highest"
      >
        <Plus size={15} />
        {t('schedule.addEntry')}
      </button>

      {/* Guard + timezone */}
      <div className="mt-5 grid grid-cols-1 sm:grid-cols-2 gap-4">
        <label className="block">
          <span className="font-label-caps text-label-caps text-on-surface-variant">{t('schedule.skipThreshold')}</span>
          <input
            type="number"
            min={0}
            max={100}
            value={threshold}
            onChange={(ev) => setThreshold(ev.target.value)}
            className="mt-1 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
          />
        </label>
        <label className="block">
          <span className="font-label-caps text-label-caps text-on-surface-variant">{t('schedule.timezone')}</span>
          <select
            value={tzMatched ?? '__custom__'}
            onChange={(ev) => setTimezone(ev.target.value)}
            className="mt-1 w-full bg-surface-container-low border border-outline-variant rounded px-3 py-2 font-data-mono text-sm text-on-surface"
          >
            {tzMatched === null && timezone ? (
              <option value="__custom__" disabled>{t('schedule.currentTz', { tz: timezone })}</option>
            ) : null}
            {TIMEZONE_OPTIONS.map((o) => (
              <option key={o.value} value={o.value}>
                {o.label}
              </option>
            ))}
          </select>
        </label>
      </div>

      {err ? (
        <div className="mt-4 rounded border border-error/40 bg-error/10 p-3">
          <p className="font-data-mono text-[11px] text-error">{err}</p>
        </div>
      ) : null}

      <div className="mt-4 flex items-center gap-3">
        <button
          type="button"
          onClick={onSave}
          disabled={saving}
          className="inline-flex items-center gap-2 bg-primary text-on-primary px-4 py-2 rounded font-label-caps text-label-caps hover:brightness-110 disabled:opacity-50"
        >
          <Save size={15} />
          {t('schedule.save')}
        </button>
        {saved ? <span className="font-data-mono text-xs text-primary">{t('schedule.saved')}</span> : null}
      </div>
    </div>
  );
}
