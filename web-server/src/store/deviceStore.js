const devices = new Map();

function upsertTelemetry({ device_id, timestamp, metrics }) {
  const existing = devices.get(device_id) || {
    device_id,
    type: 'sensor',
    metrics: {},
  };
  existing.metrics = { ...existing.metrics, ...metrics };
  existing.lastSeen = timestamp || new Date().toISOString();
  devices.set(device_id, existing);
  return existing;
}

function applyCommand({ device_id, action }) {
  const existing = devices.get(device_id) || {
    device_id,
    type: 'actuator',
    metrics: {},
  };
  existing.type = 'actuator';
  existing.metrics = { ...existing.metrics, ...action };
  existing.lastCommand = action;
  existing.lastSeen = new Date().toISOString();
  devices.set(device_id, existing);
  return existing;
}

function listDevices() {
  return Array.from(devices.values());
}

module.exports = { upsertTelemetry, applyCommand, listDevices };
