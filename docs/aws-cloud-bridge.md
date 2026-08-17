# AWS Cloud Bridge

Design detail for the off-LAN monitoring/control pipeline: `web-server`'s
`src/cloud/awsIotBridge.js` (hub side) and `smartfarm-cloud/` (AWS side, Amplify Gen2 +
Next.js). See [`architecture.md`](architecture.md) for how this fits into the whole
system, and [`aws-iot-setup.md`](aws-iot-setup.md) for the step-by-step provisioning
instructions. This feature is **additive** — the hub runs identically with or without it.

## Why it exists

`web-server` is deliberately LAN-only: in-memory state, no internet exposure. That's
correct for local irrigation control, but it means the farm can't be viewed or
controlled from off-LAN. This bridge adds that without weakening the local guarantees:
if AWS is unreachable, the hub is completely unaffected — no retry queues, no blocked
requests, nothing waits on the cloud.

## Architecture

```
Browser (anywhere) ──HTTPS──▶ Next.js dashboard (Amplify Hosting, Cognito-gated)
                                   │
                                   ▼
                   Lambda (dashboard-api): history read / command write
                        │                              │
                        ▼                              ▼
                   DynamoDB                    AWS IoT Device Shadow
              (telemetry, 90d TTL)              (UpdateThingShadow)
                        ▲                              │
                        │                              │ MQTT (mutual TLS, :8883)
                   Lambda (ingest-telemetry)            ▼
                        ▲                    ┌─────────────────────────┐
                   IoT Topic Rule             │ web-server (hub)         │
                   SELECT * FROM              │ src/cloud/awsIotBridge.js│
                   'farms/+/telemetry'        └─────────────────────────┘
                        ▲                          │              │
                        └──────────────────────────┘              │
                         MQTT publish                   pumpControl.command() /
                         farms/{hubId}/telemetry         deviceStore.applyCommand()
```

## Wire contract

This is the part that actually has to match between the two independently-deployed
halves — it's documented here as the source of truth precisely because it's easy for
the two sides to drift.

**Telemetry, hub → cloud.** Published on every `deviceStore` update (real-time, not
batched) to MQTT topic `farms/{hubId}/telemetry`, QoS 0:

```json
{
  "hubId": "jetson-01",
  "at": "2026-08-17T12:00:00.000Z",
  "devices": [ /* deviceStore.listDevices() — full device array, not a diff */ ]
}
```

The IoT Rule (`smartfarm-cloud/amplify/backend.ts`) selects `topic(2) AS hubId` from
the topic itself rather than trusting the body, so the hub's payload doesn't need to
duplicate its own id. `ingest-telemetry` currently writes one row per rule invocation
keyed by `hubId`+`timestamp` — if you change the publish payload shape, update
`ingest-telemetry/handler.ts`'s `TelemetryRuleEvent` interface too.

**Commands, cloud → hub.** The cloud writes **desired** state via
`UpdateThingShadowCommand`, keyed by `device_id` (`dashboard-api/handler.ts`,
`sendPumpCommand`):

```json
{ "state": { "desired": { "main-pump": { "state": "on" } } } }
```

AWS IoT's shadow service diffs this against reported state and publishes the
difference to `$aws/things/{thingName}/shadow/update/delta`, which looks like:

```json
{ "state": { "main-pump": { "state": "on" } }, "version": 12, "timestamp": 1700000000 }
```

**`msg.state` is keyed directly by `device_id`** — the delta does not re-wrap it in
`desired`. `awsIotBridge.js`'s `onShadowDelta` iterates `Object.entries(msg.state)`,
executes each `[deviceId, action]` pair independently (so one shadow update can carry
commands for multiple devices), and reports the outcome back:

```json
{ "state": { "reported": { "main-pump": { "state": "on", "ok": true, "relay_status": "ON", "at": "…" } } } }
```

*(This document reflects the corrected implementation. An earlier version of the hub
bridge expected a `{state: {command: {device_id, action}}}` RPC-style envelope instead
of this keyed-by-device-id shape — a real integration bug from the two halves being
built independently, caught and fixed while writing this doc, before the mismatch ever
shipped.)*

## Command routing (device_id dispatch)

Only the pump has a real hardware-relay path today. `awsIotBridge.js`'s
`executeCommand(deviceId, action)` special-cases it:

- `deviceId === pumpControl.PUMP_DEVICE_ID` ("main-pump") → `pumpControl.command(action.state, {source: 'cloud'})`. This is the **same** path `POST /api/v1/pump/control` uses — full safety logic (auto-off timer, pump-log entry, and an explicit `irrigation.auto` guard mirrored from `src/routes/pump.js` since `pumpControl.command()` itself is shared with the scheduler and can't refuse "on" unconditionally).
- Anything else → `deviceStore.applyCommand({device_id, action})`. This updates the dashboard's displayed state only — there's no hardware behind it yet, honestly matching what the local `POST /api/v1/control` endpoint already does. A future actuator gets real cloud control by adding its own relay route (the same way the pump has one), not by changing the bridge.

## Config (hub side)

Env vars, all required together or the bridge silently stays disabled (`AWS_IOT_*`
missing/invalid → one log line, local operation unaffected):

| Var | Purpose |
|---|---|
| `AWS_IOT_ENDPOINT` | Account's IoT Core data-plane endpoint (`aws iot describe-endpoint`) |
| `AWS_IOT_CERT_PATH` / `AWS_IOT_KEY_PATH` / `AWS_IOT_CA_PATH` | X.509 device cert, private key, root CA — files on the hub |
| `AWS_IOT_THING_NAME` | This hub's registered IoT Thing name (also the Shadow name and MQTT client ID) |
| `AWS_IOT_HUB_ID` | Defaults to `AWS_IOT_THING_NAME` — split only if a Thing is ever renamed independent of its farm/hub identity |
| `AWS_IOT_MQTT_PORT` | Defaults to `8883` |

## Provisioning (one-time, manual, per hub — can't be automated from a laptop)

Full step-by-step commands are in [`aws-iot-setup.md`](aws-iot-setup.md). Summary:

1. Register the hub as an AWS IoT **Thing**, generate its X.509 cert + private key.
2. Attach `smartfarm-cloud`'s `HubIotPolicy` (in `amplify/backend.ts` — scoped via the
   `${iot:Connection.Thing.ThingName}` policy variable, so every hub's cert is
   automatically confined to its own `farms/{hubId}/*` and shadow topics without a
   per-hub policy to author).
3. Copy the cert/key/root-CA onto the physical hub; set the env vars above.

## Deliberate v1 limitations

- **Single hub.** Everything is keyed by `hubId`/`thingName` so a second hub is
  provisioning, not a schema or code change — but only one is deployed today.
- **Pump-only real actuation**, as above — this is a routing decision, not a schema
  limitation, so it's cheap to extend later.
- **No cloud-side retry queue.** Device Shadow's desired/reported diffing already
  handles "hub was briefly offline" — a hand-rolled queue would duplicate that.
- **90-day telemetry TTL** in DynamoDB (`smartfarm-cloud/amplify/backend.ts`,
  `timeToLiveAttribute: 'ttl'`), not indefinite retention.
