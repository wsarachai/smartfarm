import { useSelector } from 'react-redux';
import { useGetHealthQuery } from '../features/health/healthApi';
import { selectAllDevices } from '../features/devices/devicesSlice';
import { freshness } from '../lib/freshness';
import Led from './Led';

const HEALTH_POLL_MS = 5000;

function formatUptime(seconds) {
  if (seconds == null) return '—';
  const s = Math.floor(seconds);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${s % 60}s`;
}

export default function StatusHeader() {
  const { data: health } = useGetHealthQuery(undefined, { pollingInterval: HEALTH_POLL_MS });
  const devices = useSelector(selectAllDevices);

  const online = health?.status === 'ok';
  const allFresh = devices.length > 0 && devices.every((d) => freshness(d.lastSeen) === 'online');
  const systemStatus = !online ? 'OFFLINE' : devices.length === 0 ? 'STANDBY' : allFresh ? 'NOMINAL' : 'DEGRADED';
  const nodeStatus = online ? 'online' : 'offline';

  return (
    <div className="mb-8 flex flex-col md:flex-row md:items-end justify-between gap-4">
      <div>
        <span className="font-label-caps text-label-caps text-secondary-container tracking-[0.2em] mb-1 block">
          SYSTEM STATUS: {systemStatus}
        </span>
        <h2 className="font-display-lg text-display-lg text-on-background">Live Operations</h2>
      </div>
      <div className="flex gap-2">
        <div className="panel px-4 py-2 flex items-center gap-3">
          <Led status={nodeStatus} />
          <span className="font-data-mono text-data-mono text-on-surface">
            EDGE_NODE_01: {online ? 'ACTIVE' : 'DOWN'}
          </span>
          <span className="font-data-mono text-data-mono text-on-surface-variant">
            {formatUptime(health?.uptime)}
          </span>
        </div>
      </div>
    </div>
  );
}
