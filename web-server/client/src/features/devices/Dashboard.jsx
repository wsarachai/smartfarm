import { useSelector } from 'react-redux';
import { useT } from '../../i18n';
import { useGetDevicesQuery } from './devicesApi';
import { selectAllDevices } from './devicesSlice';
import SensorCard from './SensorCard';
import ControlCard from './ControlCard';
import { compareDevices } from './deviceCategory';
import TrendChart from './TrendChart';
import CameraStatusCard from '../camera/CameraStatusCard';
import PumpControlCard from '../pump/PumpControlCard';
import WaterStressCard from '../insights/WaterStressCard';
import DiseaseCard from '../insights/DiseaseCard';

const POLL_INTERVAL_MS = 5000;

function EmptyPanel({ children }) {
  return (
    <div className="panel p-5 flex items-center justify-center text-on-surface-variant font-data-mono text-xs min-h-[120px]">
      {children}
    </div>
  );
}

export default function Dashboard() {
  const t = useT();
  useGetDevicesQuery(undefined, { pollingInterval: POLL_INTERVAL_MS });
  const devices = useSelector(selectAllDevices);
  // Order the widgets by category (sensor -> pump -> camera -> other), stable by
  // device_id, so the grid isn't shuffled by arbitrary Map insertion order.
  const sensors = devices.filter((d) => d.type !== 'actuator').sort(compareDevices);
  // 'main-pump' is rendered by the dedicated PumpControlCard below; keep it out
  // of the generic list so it isn't shown twice.
  const actuators = devices.filter((d) => d.type === 'actuator' && d.device_id !== 'main-pump');

  // Widget blocks flow in reading order: pump -> camera -> sensors -> AI ->
  // other. Each is a consistent grid cell so the responsive auto-flow stays
  // clean (2-up on md, 3-up on lg); the live trend spans full width at the end.
  return (
    <div className="grid grid-cols-1 md:grid-cols-12 gap-gutter">
      {/* 1. Pump — dedicated pump-zone control (configured in Settings). */}
      <div className="md:col-span-6 lg:col-span-4">
        <PumpControlCard />
      </div>

      {/* 2. Water Stress Risk (AI insight). */}
      <div className="md:col-span-6 lg:col-span-4">
        <WaterStressCard />
      </div>

      {/* 3. Sensor telemetry cards — one per sensor device. */}
      {sensors.length === 0 ? (
        <div className="md:col-span-6 lg:col-span-4">
          <EmptyPanel>{t('dashboard.noSensors')}</EmptyPanel>
        </div>
      ) : (
        sensors.map((device) => (
          <div key={device.device_id} className="md:col-span-6 lg:col-span-4">
            <SensorCard device={device} />
          </div>
        ))
      )}

      {/* 4. Live trend (client-buffered), full width — sits with the sensor
          data it plots: "current readings -> their trend over time". */}
      <div className="md:col-span-12">
        <TrendChart />
      </div>

      {/* 5. Camera status + remaining AI insight. */}
      <div className="md:col-span-6 lg:col-span-4">
        <CameraStatusCard />
      </div>
      <div className="md:col-span-6 lg:col-span-4">
        <DiseaseCard />
      </div>

      {/* 6. Other actuators. */}
      {actuators.map((device) => (
        <div key={device.device_id} className="md:col-span-6 lg:col-span-4">
          <ControlCard device={device} />
        </div>
      ))}
    </div>
  );
}
