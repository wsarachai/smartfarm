import { useState } from 'react';
import { useSelector } from 'react-redux';
import { useGetCameraStatusQuery } from './cameraApi';
import { selectCameraStatus } from './cameraSlice';

const STATUS_POLL_MS = 5000;

export default function CameraCard() {
  // Poll only the tiny status JSON; the MJPEG <img> streams on its own.
  useGetCameraStatusQuery(undefined, { pollingInterval: STATUS_POLL_MS });
  const { online, hasFrame, ageMs, bytes } = useSelector(selectCameraStatus);
  const [imgError, setImgError] = useState(false);

  const state = imgError || !hasFrame ? 'offline' : online ? 'live' : 'stale';

  return (
    <div className="device-card camera-card">
      <div className="camera-header">
        <h3>ESP32-CAM</h3>
        <span className={`camera-badge camera-badge--${state}`}>{state.toUpperCase()}</span>
      </div>

      <div className="camera-frame">
        {hasFrame && !imgError ? (
          <img
            src="/api/v1/camera/stream"
            alt="ESP32-CAM live stream"
            onError={() => setImgError(true)}
          />
        ) : (
          <div className="camera-placeholder">No frames received yet</div>
        )}
      </div>

      <ul className="metrics-list">
        <li>
          <span className="metric-key">last frame</span>
          <span className="metric-value">{ageMs == null ? '—' : `${Math.round(ageMs / 1000)}s ago`}</span>
        </li>
        <li>
          <span className="metric-key">size</span>
          <span className="metric-value">{bytes ? `${(bytes / 1024).toFixed(0)} KB` : '—'}</span>
        </li>
      </ul>
    </div>
  );
}
