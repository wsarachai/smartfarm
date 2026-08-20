import mqtt, { type MqttClient } from 'mqtt';
import type { Prisma } from '@prisma/client';
import { db } from './db';

// Long-running MQTT subscriber for the self-hosted broker (replaces AWS IoT
// Core + Device Shadow). Started once per server process via
// instrumentation.ts's register() hook — NOT per-request, so this needs a
// module-level singleton the same way lib/db.ts's PrismaClient does.
//
// Wire contract (see docs/mqtt-cloud-bridge.md):
//   farms/{hubId}/telemetry        hub -> here,  {hubId, device_id, timestamp, metrics}
//   farms/{hubId}/command          here -> hub,  {deviceId, action: {state}}
//   farms/{hubId}/command/result   hub -> here,  {deviceId, ok, ...outcome, at}
//
// Fire-and-forget by design (see docs/mqtt-cloud-bridge.md's "deliberate v1
// limitations" — no Device Shadow equivalent): a command published while the
// hub is offline is simply lost, not queued or redelivered.

type LatestReading = { deviceId: string; timestamp: string; metrics: Record<string, unknown> };
type CommandResult = { deviceId: string; ok: boolean; error?: string; at: string; [k: string]: unknown };

// Everything server-lifetime lives on globalThis, not plain module-level
// consts — Next.js can load separate bundled instances of this module for
// instrumentation.ts (where telemetry is written) vs. API routes (where it's
// read back), which would otherwise split a module-level Map into two
// independent copies: writes landing in one, reads coming from an empty
// other. Caught live on itsci-data.local: telemetry was confirmed arriving
// and parsing correctly (broker + hub logs both clean), but
// GET /api/telemetry/latest always returned []. globalThis is a true
// process-wide object regardless of which bundle chunk touches it, same
// reasoning as lib/db.ts's PrismaClient singleton.
const globalForMqtt = globalThis as unknown as {
  mqttClient?: MqttClient;
  latestByHub?: Map<string, Map<string, LatestReading>>;
  lastResultByHub?: Map<string, Map<string, CommandResult>>;
};

// deviceId -> latest reading, per hub. Filled from every telemetry message so
// GET /api/telemetry/latest can answer without a DB round trip on every poll.
const latestByHub = globalForMqtt.latestByHub ?? (globalForMqtt.latestByHub = new Map());

// deviceId -> most recent command outcome, per hub. Best-effort, in-memory
// only — not persisted, since it's just for the dashboard to show "last
// command: ok/failed" rather than an audit trail.
const lastResultByHub = globalForMqtt.lastResultByHub ?? (globalForMqtt.lastResultByHub = new Map());

const LOG_PREFIX = '[mqtt]';

function commandTopic(hubId: string) {
  return `farms/${hubId}/command`;
}

function handleTelemetry(hubId: string, payloadBuf: Buffer) {
  let msg: { device_id?: string; timestamp?: string; metrics?: Record<string, unknown> };
  try {
    msg = JSON.parse(payloadBuf.toString());
  } catch (err) {
    console.error(`${LOG_PREFIX} malformed telemetry payload: ${(err as Error).message}`);
    return;
  }
  if (!msg.device_id) return;

  const timestamp = msg.timestamp ?? new Date().toISOString();
  const metrics = msg.metrics ?? {};

  if (!latestByHub.has(hubId)) latestByHub.set(hubId, new Map());
  latestByHub.get(hubId)!.set(msg.device_id, { deviceId: msg.device_id, timestamp, metrics });
  // Success-path log, deliberately kept (not just error-path) — its absence
  // is what made the globalThis bug above take this long to isolate: every
  // layer looked healthy from its own error logs alone.
  console.log(`${LOG_PREFIX} telemetry: ${hubId}/${msg.device_id}`);

  db.telemetryReading
    .create({
      data: {
        hubId,
        deviceId: msg.device_id,
        timestamp: new Date(timestamp),
        metrics: metrics as Prisma.InputJsonValue,
      },
    })
    .catch((err) => console.error(`${LOG_PREFIX} failed to persist telemetry: ${err.message}`));
}

function handleCommandResult(hubId: string, payloadBuf: Buffer) {
  let msg: CommandResult;
  try {
    msg = JSON.parse(payloadBuf.toString());
  } catch (err) {
    console.error(`${LOG_PREFIX} malformed command result payload: ${(err as Error).message}`);
    return;
  }
  if (!msg.deviceId) return;

  if (!lastResultByHub.has(hubId)) lastResultByHub.set(hubId, new Map());
  lastResultByHub.get(hubId)!.set(msg.deviceId, msg);
}

// farms/{hubId}/telemetry or farms/{hubId}/command/result -> hubId, or null
// if the topic doesn't match either shape (e.g. a stray retained message).
function parseHubTopic(topic: string): { hubId: string; kind: 'telemetry' | 'command-result' } | null {
  const parts = topic.split('/');
  if (parts.length === 3 && parts[0] === 'farms' && parts[2] === 'telemetry') {
    return { hubId: parts[1], kind: 'telemetry' };
  }
  if (parts.length === 4 && parts[0] === 'farms' && parts[2] === 'command' && parts[3] === 'result') {
    return { hubId: parts[1], kind: 'command-result' };
  }
  return null;
}

function connect(): MqttClient {
  const url = process.env.MQTT_URL;
  const username = process.env.MQTT_USERNAME;
  const password = process.env.MQTT_PASSWORD;

  if (!url || !username || !password) {
    throw new Error('MQTT_URL / MQTT_USERNAME / MQTT_PASSWORD must all be set');
  }

  const client = mqtt.connect(url, {
    username,
    password,
    clientId: `smartfarm-cloud-${Math.random().toString(16).slice(2, 10)}`,
    clean: true,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
  });

  client.on('connect', () => {
    console.log(`${LOG_PREFIX} connected to ${url}`);
    client.subscribe(['farms/+/telemetry', 'farms/+/command/result'], { qos: 0 }, (err) => {
      if (err) console.error(`${LOG_PREFIX} subscribe failed: ${err.message}`);
    });
  });

  client.on('reconnect', () => console.log(`${LOG_PREFIX} reconnecting…`));
  client.on('close', () => console.log(`${LOG_PREFIX} connection closed`));
  client.on('error', (err) => console.error(`${LOG_PREFIX} connection error: ${err.message}`));

  client.on('message', (topic, payloadBuf) => {
    const parsed = parseHubTopic(topic);
    if (!parsed) return;
    if (parsed.kind === 'telemetry') handleTelemetry(parsed.hubId, payloadBuf);
    else handleCommandResult(parsed.hubId, payloadBuf);
  });

  return client;
}

export function startMqtt(): void {
  if (globalForMqtt.mqttClient) return;
  globalForMqtt.mqttClient = connect();
}

export function publishCommand(hubId: string, deviceId: string, state: string): { ok: boolean; error?: string } {
  const client = globalForMqtt.mqttClient;
  if (!client || !client.connected) {
    return { ok: false, error: 'not connected to broker' };
  }
  client.publish(commandTopic(hubId), JSON.stringify({ deviceId, action: { state } }), { qos: 0 });
  return { ok: true };
}

export function getLatestReadings(hubId: string): LatestReading[] {
  return Array.from(latestByHub.get(hubId)?.values() ?? []);
}

export function getLastCommandResult(hubId: string, deviceId: string): CommandResult | undefined {
  return lastResultByHub.get(hubId)?.get(deviceId);
}

export function isMqttConnected(): boolean {
  return Boolean(globalForMqtt.mqttClient?.connected);
}
