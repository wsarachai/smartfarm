import { useEffect, useState } from 'react';
import { Power, Timer } from 'lucide-react';
import Led from '../../components/Led';
import { usePumpSettings } from './pumpSettings';
import { useGetPumpStatusQuery, useSetPumpMutation } from './pumpApi';
import { useGetSettingsQuery } from '../settings/settingsApi';

const POLL_INTERVAL_MS = 5000;

// Local mm:ss countdown driven off the backend's autoOffAt timestamp. The
// backend owns the actual auto-off; this only visualizes the remaining time.
function useCountdown(autoOffAt) {
  const [remainingMs, setRemainingMs] = useState(0);
  useEffect(() => {
    if (!autoOffAt) {
      setRemainingMs(0);
      return undefined;
    }
    const target = new Date(autoOffAt).getTime();
    const tick = () => setRemainingMs(Math.max(0, target - Date.now()));
    tick();
    const id = setInterval(tick, 1000);
    return () => clearInterval(id);
  }, [autoOffAt]);
  return remainingMs;
}

function formatMs(ms) {
  const total = Math.ceil(ms / 1000);
  const m = Math.floor(total / 60);
  const s = total % 60;
  return `${m}:${String(s).padStart(2, '0')}`;
}

export default function PumpControlCard() {
  const settings = usePumpSettings();
  const { data: appSettings } = useGetSettingsQuery();
  const auto = Boolean(appSettings?.irrigation?.auto);
  const { data } = useGetPumpStatusQuery(undefined, {
    pollingInterval: POLL_INTERVAL_MS,
  });
  const [setPump, { isLoading }] = useSetPumpMutation();

  const online = data?.online === true;
  const isOn = online && data?.relay_status === 'ON';
  const remainingMs = useCountdown(isOn ? data?.autoOffAt : null);

  const stateLabel = data == null ? '…' : !online ? 'OFFLINE' : isOn ? 'ON' : 'OFF';
  const ledStatus = isOn ? 'online' : 'offline';

  const command = (state) => setPump({ state });

  // In AUTO mode the schedule owns the pump: manual ON is disabled (server also
  // refuses it with 409); manual OFF stays available as an emergency stop.
  const onDisabled = isLoading || isOn || auto;
  const offDisabled = isLoading || (online && !isOn);

  return (
    <div className="panel p-5">
      <div className="flex items-center justify-between gap-4">
        <div className="flex items-center gap-4 min-w-0">
          <div className="w-12 h-12 shrink-0 bg-primary-container/20 flex items-center justify-center border border-primary/30">
            <Power size={22} className="text-primary" />
          </div>
          <div className="min-w-0">
            <h4 className="font-headline-sm text-headline-sm text-on-surface break-all">
              {settings.label}
            </h4>
            <div className="flex items-center gap-2">
              <Led status={ledStatus} size="w-2 h-2" />
              <span
                className={`font-label-caps text-label-caps ${
                  isOn ? 'text-primary' : 'text-on-surface-variant'
                }`}
              >
                {stateLabel}
              </span>
            </div>
          </div>
        </div>

        {isOn && data?.autoOffAt ? (
          <div className="flex items-center gap-1.5 text-tertiary shrink-0" title="Auto-off in">
            <Timer size={14} />
            <span className="font-data-mono text-sm tabular-nums">{formatMs(remainingMs)}</span>
          </div>
        ) : null}
      </div>

      <div className="mt-4 grid grid-cols-2 gap-3">
        <button
          type="button"
          onClick={() => command('on')}
          disabled={onDisabled}
          className="bg-primary text-on-primary px-4 py-2 font-label-caps text-label-caps font-bold transition-transform active:scale-95 disabled:opacity-40 disabled:active:scale-100"
        >
          ON
        </button>
        <button
          type="button"
          onClick={() => command('off')}
          disabled={offDisabled}
          className="bg-surface-container-high border border-outline-variant text-on-surface px-4 py-2 font-label-caps text-label-caps font-bold transition-transform active:scale-95 hover:bg-surface-container-highest disabled:opacity-40 disabled:active:scale-100"
        >
          OFF
        </button>
      </div>

      {auto ? (
        <p className="mt-3 font-data-mono text-[11px] text-tertiary">
          Auto mode on — the schedule controls the pump. Switch to Manual on the Irrigation page to run it by hand.
        </p>
      ) : null}

      {data && !online ? (
        <p className="mt-3 font-data-mono text-[11px] text-error">
          Pump unreachable at {settings.url} — check the node is powered and joined the AP.
        </p>
      ) : null}
    </div>
  );
}
