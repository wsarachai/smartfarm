import { Power } from 'lucide-react';
import Led from '../../components/Led';
import { useT } from '../../i18n';
import { useSendCommandMutation } from './devicesApi';
import { freshness } from '../../lib/freshness';

// Actuator control card (the wireframe's "Main Pump"). Boolean metrics are the
// switchable channels; TOGGLE flips them all. Works for a single-relay pump or
// a multi-channel actuator alike.
export default function ControlCard({ device }) {
  const t = useT();
  const [sendCommand, { isLoading }] = useSendCommandMutation();
  const status = freshness(device.lastSeen);

  const bools = Object.entries(device.metrics || {}).filter(([, v]) => typeof v === 'boolean');
  const anyOn = bools.some(([, v]) => v);
  const stateLabel = bools.length === 0 ? 'N/A' : anyOn ? 'ON' : 'OFF';

  const toggle = () => {
    if (bools.length === 0) return;
    const next = !anyOn;
    const action = Object.fromEntries(bools.map(([k]) => [k, next]));
    sendCommand({ device_id: device.device_id, action });
  };

  return (
    <div className="panel p-5 flex items-center justify-between gap-4">
      <div className="flex items-center gap-4 min-w-0">
        <div className="w-12 h-12 shrink-0 bg-primary-container/20 flex items-center justify-center border border-primary/30">
          <Power size={22} className="text-primary" />
        </div>
        <div className="min-w-0">
          <h4 className="font-headline-sm text-headline-sm text-on-surface break-all">{device.device_id}</h4>
          <div className="flex items-center gap-2">
            <Led status={anyOn ? status : 'offline'} size="w-2 h-2" />
            <span className={`font-label-caps text-label-caps ${anyOn ? 'text-primary' : 'text-on-surface-variant'}`}>
              {stateLabel}
            </span>
          </div>
        </div>
      </div>
      <button
        type="button"
        onClick={toggle}
        disabled={bools.length === 0 || isLoading}
        className="bg-primary text-on-primary px-4 py-2 font-label-caps text-label-caps font-bold transition-transform active:scale-95 disabled:opacity-40 disabled:active:scale-100"
      >
        {t('control.toggle')}
      </button>
    </div>
  );
}
