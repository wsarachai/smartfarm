import { useSelector } from 'react-redux';
import { useGetDevicesQuery } from './devicesApi';
import { selectAllDevices } from './devicesSlice';
import DeviceCard from './DeviceCard';

const POLL_INTERVAL_MS = 5000;

export default function Dashboard() {
  useGetDevicesQuery(undefined, { pollingInterval: POLL_INTERVAL_MS });
  const devices = useSelector(selectAllDevices);

  return (
    <div className="dashboard-grid">
      {devices.length === 0 && <p>No devices reporting yet.</p>}
      {devices.map((device) => (
        <DeviceCard key={device.device_id} device={device} />
      ))}
    </div>
  );
}
