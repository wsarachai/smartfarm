import { useSelector } from 'react-redux';
import { useGetDevicesQuery } from './devicesApi';
import { selectAllDevices } from './devicesSlice';
import SensorCard from './SensorCard';
import ControlCard from './ControlCard';
import TrendChart from './TrendChart';
import CameraStatusCard from '../camera/CameraStatusCard';
import PumpControlCard from '../pump/PumpControlCard';
import AiInsightCard from '../insights/AiInsightCard';

const POLL_INTERVAL_MS = 5000;

function EmptyPanel({ children }) {
  return (
    <div className="panel p-5 flex items-center justify-center text-on-surface-variant font-data-mono text-xs min-h-[120px]">
      {children}
    </div>
  );
}

export default function Dashboard() {
  useGetDevicesQuery(undefined, { pollingInterval: POLL_INTERVAL_MS });
  const devices = useSelector(selectAllDevices);
  const sensors = devices.filter((d) => d.type !== 'actuator');
  // 'main-pump' is rendered by the dedicated PumpControlCard below; keep it out
  // of the generic list so it isn't shown twice.
  const actuators = devices.filter((d) => d.type === 'actuator' && d.device_id !== 'main-pump');

  return (
    <div className="grid grid-cols-1 md:grid-cols-12 gap-gutter">
      {/* Sensor telemetry cards — one per sensor device. */}
      {sensors.length === 0 ? (
        <div className="md:col-span-6 lg:col-span-4">
          <EmptyPanel>NO SENSORS REPORTING</EmptyPanel>
        </div>
      ) : (
        sensors.map((device) => (
          <div key={device.device_id} className="md:col-span-6 lg:col-span-4">
            <SensorCard device={device} />
          </div>
        ))
      )}

      {/* Hardware controls column: actuators + camera status. */}
      <div className="md:col-span-12 lg:col-span-4 grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-1 gap-4">
        {/* Dedicated pump-zone control (configured in Settings) stands in for the
            generic actuator empty-state — the pump is the actuator operators see. */}
        <PumpControlCard />
        {actuators.map((device) => (
          <ControlCard key={device.device_id} device={device} />
        ))}
        <CameraStatusCard />
      </div>

      {/* Live trend (client-buffered) + AI preview. */}
      <div className="md:col-span-12 lg:col-span-8">
        <TrendChart />
      </div>
      <div className="md:col-span-12 lg:col-span-4">
        <AiInsightCard />
      </div>
    </div>
  );
}
