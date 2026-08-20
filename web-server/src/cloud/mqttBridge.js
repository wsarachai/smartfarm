// Self-hosted MQTT cloud bridge — hub-side half of the pipeline described in
// docs/mqtt-cloud-bridge.md. Publishes real-time telemetry up via
// deviceStore.setTelemetryListener and executes commands delivered on this
// hub's command topic. Reuses pumpControl/deviceStore as the only actuation
// paths — no parallel command logic lives here.
//
// Replaces the earlier AWS IoT Core bridge (awsIotBridge.js, mutual-TLS X.509
// + Device Shadow) with a plain username/password MQTT connection to a
// self-hosted broker, over wss:// through a Cloudflare Tunnel — the hub is a
// physically separate machine reaching the broker over the public internet,
// same public endpoint the browser dashboard uses (see the doc for why: no
// new inbound port, no dedicated MQTTS listener to secure).
//
// No Device Shadow equivalent, so no offline command queueing — this is a
// deliberate v1 limitation (see the doc), not an oversight.
//
// Graceful degrade: any load/connect/publish/command failure is caught and
// logged here — never thrown into a local request path. If config is
// missing/invalid the bridge simply never starts; local dashboard,
// irrigation scheduler, and manual pump control are untouched either way.

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

const LOG_PREFIX = '[mqtt-bridge]';

let client = null;
let connected = false;
let started = false;

function loadConfig() {
  const url = process.env.MQTT_URL;
  const username = process.env.MQTT_USERNAME;
  const password = process.env.MQTT_PASSWORD;
  const hubId = process.env.MQTT_HUB_ID;

  if (!url || !username || !password || !hubId) {
    return null;
  }

  return { url, username, password, hubId };
}

function topics(hubId) {
  return {
    telemetry: `farms/${hubId}/telemetry`,
    command: `farms/${hubId}/command`,
    commandResult: `farms/${hubId}/command/result`,
  };
}

// One MQTT publish per device, matching the cloud side's expected
// {hubId, device_id, timestamp, metrics} shape exactly (see
// docs/mqtt-cloud-bridge.md's wire contract).
function publishTelemetry(cfg, t) {
  if (!connected) return; // don't buffer real-time readings across an outage
  deviceStore.listDevices().forEach((device) => {
    const payload = {
      hubId: cfg.hubId,
      device_id: device.device_id,
      timestamp: device.lastSeen || new Date().toISOString(),
      metrics: device.metrics || {},
    };
    client.publish(t.telemetry, JSON.stringify(payload), { qos: 0 }, (err) => {
      if (err) console.error(`${LOG_PREFIX} telemetry publish failed: ${err.message}`);
    });
  });
}

function reportResult(t, deviceId, outcome) {
  if (!client) return;
  const payload = JSON.stringify({ deviceId, ...outcome, at: new Date().toISOString() });
  client.publish(t.commandResult, payload, { qos: 0 }, (err) => {
    if (err) console.error(`${LOG_PREFIX} command result publish failed: ${err.message}`);
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

// Command payload is {deviceId, action} (see docs/mqtt-cloud-bridge.md) — a
// flat, direct publish, not a Device-Shadow-style desired/reported tree.
function onCommand(t, payloadBuf) {
  let msg;
  try {
    msg = JSON.parse(payloadBuf.toString());
  } catch (err) {
    console.error(`${LOG_PREFIX} malformed command payload: ${err.message}`);
    return;
  }

  const { deviceId, action } = msg || {};
  if (!deviceId || !action || typeof action !== 'object') return;

  executeCommand(deviceId, action)
    .then((outcome) => reportResult(t, deviceId, outcome))
    .catch((err) => {
      console.error(`${LOG_PREFIX} command execution error: ${err.message}`);
      reportResult(t, deviceId, { ok: false, error: err.message });
    });
}

function connect(cfg) {
  const t = topics(cfg.hubId);

  client = mqtt.connect(cfg.url, {
    username: cfg.username,
    password: cfg.password,
    clientId: `hub-${cfg.hubId}`,
    clean: true,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
  });

  client.on('connect', () => {
    connected = true;
    console.log(`${LOG_PREFIX} connected to ${cfg.url} as "${cfg.hubId}"`);
    client.subscribe(t.command, { qos: 0 }, (err) => {
      if (err) console.error(`${LOG_PREFIX} command subscribe failed: ${err.message}`);
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
      if (topic === t.command) onCommand(t, payloadBuf);
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
    console.log(`${LOG_PREFIX} not configured (MQTT_* env vars missing) — bridge disabled, local control unaffected`);
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
