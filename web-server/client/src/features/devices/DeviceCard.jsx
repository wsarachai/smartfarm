import { useSendCommandMutation } from './devicesApi';

function SensorReadout({ metrics }) {
  return (
    <ul className="metrics-list">
      {Object.entries(metrics).map(([key, value]) => (
        <li key={key}>
          <span className="metric-key">{key}</span>
          <span className="metric-value">{String(value)}</span>
        </li>
      ))}
    </ul>
  );
}

function ActuatorControls({ deviceId, metrics }) {
  const [sendCommand] = useSendCommandMutation();

  const toggle = (key) => {
    sendCommand({ device_id: deviceId, action: { [key]: !metrics[key] } });
  };

  return (
    <div className="actuator-controls">
      {Object.entries(metrics).map(([key, value]) => (
        <button key={key} type="button" onClick={() => toggle(key)}>
          {key}: {value ? 'ON' : 'OFF'}
        </button>
      ))}
    </div>
  );
}

export default function DeviceCard({ device }) {
  const { device_id: deviceId, type, metrics } = device;

  return (
    <div className="device-card">
      <h3>{deviceId}</h3>
      {type === 'actuator' ? (
        <ActuatorControls deviceId={deviceId} metrics={metrics} />
      ) : (
        <SensorReadout metrics={metrics} />
      )}
    </div>
  );
}
