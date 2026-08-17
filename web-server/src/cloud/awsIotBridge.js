// AWS IoT Core bridge — hub-side half of the cloud pipeline described in
// docs (see the aws-cloud-bridge plan). Publishes real-time telemetry up via
// deviceStore.setTelemetryListener and executes commands delivered through
// the hub's classic Device Shadow delta. Reuses pumpControl/deviceStore as
// the only actuation paths — no parallel command logic lives here.
//
// Uses plain `mqtt` (pure JS, no native bindings) rather than
// aws-iot-device-sdk-v2: that SDK pulls in aws-crt, a native addon that
// needs a prebuild (or a full C++ toolchain) per target arch, which is a
// poor fit for old Ubuntu 18.04 Jetson Nano / Raspberry Pi glibc baselines.
// AWS IoT Core's mutual-TLS port (8883) speaks standard MQTT 3.1.1, and
// Device Shadow topics are just reserved MQTT topics ($aws/things/...), so
// no AWS-specific client is actually required.
//
// Graceful degrade: any load/connect/publish/command failure is caught and
// logged here — never thrown into a local request path. If config is
// missing/invalid the bridge simply never starts; local dashboard,
// irrigation scheduler, and manual pump control are untouched either way.

const fs = require('fs');
const mqtt = require('mqtt');

const deviceStore = require('../store/deviceStore');
const pumpControl = require('../store/pumpControl');
const settingsStore = require('../store/settingsStore');
// deviceStore.setTelemetryListener is a single-slot callback and
// telemetryStore already occupies it (trend-history capture, wired in at its
// own require-time). Registering ours would silently clobber that slot and
// break history capture the moment this bridge connects, so we chain to its
// already-exported captureSnapshot() rather than replace it outright.
const telemetryStore = require('../store/telemetryStore');

const LOG_PREFIX = '[aws-iot]';

let client = null;
let connected = false;
let started = false;

function loadConfig() {
  const endpoint = process.env.AWS_IOT_ENDPOINT;
  const certPath = process.env.AWS_IOT_CERT_PATH;
  const keyPath = process.env.AWS_IOT_KEY_PATH;
  const caPath = process.env.AWS_IOT_CA_PATH;
  const thingName = process.env.AWS_IOT_THING_NAME;
  // Defaults to thingName: today there's exactly one hub, and the shadow
  // (keyed by thing name) and the telemetry topic (keyed by hubId) are the
  // same physical device — split only if a thing is ever renamed independent
  // of its farm/hub identity.
  const hubId = process.env.AWS_IOT_HUB_ID || thingName;
  const port = Number(process.env.AWS_IOT_MQTT_PORT) || 8883;

  if (!endpoint || !certPath || !keyPath || !caPath || !thingName) {
    return null;
  }

  let cert;
  let key;
  let ca;
  try {
    cert = fs.readFileSync(certPath);
    key = fs.readFileSync(keyPath);
    ca = fs.readFileSync(caPath);
  } catch (err) {
    console.error(`${LOG_PREFIX} failed to read cert/key/CA file(s): ${err.message}`);
    return null;
  }

  return { endpoint, port, cert, key, ca, thingName, hubId };
}

function topics(thingName) {
  return {
    telemetry: (hubId) => `farms/${hubId}/telemetry`,
    shadowDelta: `$aws/things/${thingName}/shadow/update/delta`,
    shadowUpdate: `$aws/things/${thingName}/shadow/update`,
  };
}

function publishTelemetry(cfg, t) {
  if (!connected) return; // don't buffer real-time readings across an outage
  const payload = {
    hubId: cfg.hubId,
    at: new Date().toISOString(),
    devices: deviceStore.listDevices(),
  };
  client.publish(t.telemetry(cfg.hubId), JSON.stringify(payload), { qos: 0 }, (err) => {
    if (err) console.error(`${LOG_PREFIX} telemetry publish failed: ${err.message}`);
  });
}

function reportOutcome(t, deviceId, action, outcome) {
  if (!client) return;
  const reported = {
    [deviceId]: { ...action, ...outcome, at: new Date().toISOString() },
  };
  const payload = JSON.stringify({ state: { reported } });
  client.publish(t.shadowUpdate, payload, { qos: 1 }, (err) => {
    if (err) console.error(`${LOG_PREFIX} shadow report failed: ${err.message}`);
  });
}

// Mirrors the irrigation-AUTO 409 guard in src/routes/pump.js: manual/cloud
// "on" is refused while the scheduler owns the pump, "off" is always allowed
// as an emergency stop. Not in pumpControl.command() itself (that layer is
// shared with the scheduler, which legitimately turns the pump on), so any
// caller outside the route has to apply the same guard itself.
function pumpAutoModeOn() {
  try {
    return Boolean(settingsStore.get().irrigation.auto);
  } catch {
    return false;
  }
}

async function executeCommand(deviceId, action) {
  if (deviceId === pumpControl.PUMP_DEVICE_ID) {
    const state = action.state;
    if (state === 'on' && pumpAutoModeOn()) {
      return { ok: false, error: 'auto mode is on — manual start is disabled' };
    }
    const result = await pumpControl.command(state, { source: 'cloud' });
    return result.online === false
      ? { ok: false, error: result.error || 'pump command failed' }
      : { ok: true, relay_status: result.relay_status };
  }

  const device = deviceStore.applyCommand({ device_id: deviceId, action });
  return { ok: true, device };
}

// A shadow delta's `state` is keyed by device_id, one desired sub-document per
// device (e.g. {"main-pump": {"state": "on"}}) — this is how
// smartfarm-cloud's sendPumpCommand actually writes it (UpdateThingShadow
// with state.desired[deviceId] = {...}), not a generic {command:{device_id,
// action}} RPC envelope. Each key is handled independently so one shadow
// update can carry commands for more than one device.
function onShadowDelta(t, payloadBuf) {
  let msg;
  try {
    msg = JSON.parse(payloadBuf.toString());
  } catch (err) {
    console.error(`${LOG_PREFIX} malformed shadow delta payload: ${err.message}`);
    return;
  }

  const desired = msg && msg.state;
  if (!desired || typeof desired !== 'object') return;

  Object.entries(desired).forEach(([deviceId, action]) => {
    if (!action || typeof action !== 'object') return;
    executeCommand(deviceId, action)
      .then((outcome) => reportOutcome(t, deviceId, action, outcome))
      .catch((err) => {
        console.error(`${LOG_PREFIX} command execution error: ${err.message}`);
        reportOutcome(t, deviceId, action, { ok: false, error: err.message });
      });
  });
}

function connect(cfg) {
  const t = topics(cfg.thingName);

  client = mqtt.connect({
    host: cfg.endpoint,
    port: cfg.port,
    protocol: 'mqtts',
    cert: cfg.cert,
    key: cfg.key,
    ca: cfg.ca,
    clientId: cfg.thingName,
    clean: true,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
  });

  client.on('connect', () => {
    connected = true;
    console.log(`${LOG_PREFIX} connected to ${cfg.endpoint} as "${cfg.thingName}"`);
    client.subscribe(t.shadowDelta, { qos: 1 }, (err) => {
      if (err) console.error(`${LOG_PREFIX} shadow delta subscribe failed: ${err.message}`);
    });
  });

  client.on('reconnect', () => {
    console.log(`${LOG_PREFIX} reconnecting…`);
  });

  client.on('close', () => {
    connected = false;
  });

  client.on('offline', () => {
    connected = false;
  });

  client.on('error', (err) => {
    connected = false;
    console.error(`${LOG_PREFIX} connection error: ${err.message}`);
  });

  client.on('message', (topic, payloadBuf) => {
    try {
      if (topic === t.shadowDelta) onShadowDelta(t, payloadBuf);
    } catch (err) {
      console.error(`${LOG_PREFIX} message handling error: ${err.message}`);
    }
  });

  deviceStore.setTelemetryListener(() => {
    try {
      telemetryStore.captureSnapshot();
    } catch (err) {
      console.error(`${LOG_PREFIX} telemetry-history capture error: ${err.message}`);
    }
    try {
      publishTelemetry(cfg, t);
    } catch (err) {
      console.error(`${LOG_PREFIX} telemetry publish error: ${err.message}`);
    }
  });
}

function start() {
  if (started) return;
  started = true;

  let cfg;
  try {
    cfg = loadConfig();
  } catch (err) {
    console.error(`${LOG_PREFIX} config load failed: ${err.message}`);
    return;
  }

  if (!cfg) {
    console.log(`${LOG_PREFIX} not configured (AWS_IOT_* env vars missing/invalid) — bridge disabled, local control unaffected`);
    return;
  }

  try {
    connect(cfg);
  } catch (err) {
    console.error(`${LOG_PREFIX} failed to start: ${err.message}`);
  }
}

function stop() {
  if (client) {
    client.end(true);
    client = null;
  }
  connected = false;
  started = false;
}

function isConnected() {
  return connected;
}

module.exports = { start, stop, isConnected };
