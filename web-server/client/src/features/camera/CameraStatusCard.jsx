import { useSelector } from 'react-redux';
import { Link } from 'react-router-dom';
import { Video } from 'lucide-react';
import { useGetCameraStatusQuery } from './cameraApi';
import { selectCameraStatus } from './cameraSlice';
import Led from '../../components/Led';

const STATUS_POLL_MS = 5000;

// Compact camera card for the dashboard (the full live feed lives on the
// Cameras page). Polls only the tiny status JSON.
export default function CameraStatusCard() {
  useGetCameraStatusQuery(undefined, { pollingInterval: STATUS_POLL_MS });
  const { online, hasFrame, ageMs, degrading } = useSelector(selectCameraStatus);

  const status = !hasFrame ? 'offline' : online ? 'online' : 'stale';
  const label = status === 'online' ? 'ONLINE' : status === 'stale' ? 'STALE' : 'OFFLINE';
  const age = ageMs == null ? '—' : `${Math.round(ageMs / 1000)}s ago`;

  return (
    <div className="panel p-5 flex items-center justify-between gap-4">
      <div className="flex items-center gap-4 min-w-0">
        <div className="w-12 h-12 shrink-0 bg-secondary-container/20 flex items-center justify-center border border-secondary/30">
          <Video size={22} className="text-secondary" />
        </div>
        <div className="min-w-0">
          <h4 className="font-headline-sm text-headline-sm text-on-surface">ESP32-CAM</h4>
          <div className="flex items-center gap-2">
            <Led status={status} size="w-2 h-2" />
            <span className={`font-label-caps text-label-caps ${status === 'online' ? 'text-primary' : status === 'stale' ? 'text-tertiary' : 'text-error'}`}>
              {label}
            </span>
            <span className="font-data-mono text-[10px] text-on-surface-variant">{age}</span>
          </div>
          {degrading ? (
            <span className="mt-1 inline-block font-label-caps text-label-caps text-error uppercase">
              ⚠ Degrading
            </span>
          ) : null}
        </div>
      </div>
      <Link
        to="/cameras"
        title="Open the live camera feed"
        className="border border-secondary text-secondary px-4 py-2 font-label-caps text-label-caps font-bold hover:bg-secondary/10 transition-colors active:scale-95"
      >
        VIEW
      </Link>
    </div>
  );
}
