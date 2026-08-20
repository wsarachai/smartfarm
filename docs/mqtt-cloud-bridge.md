# Self-Hosted MQTT Cloud Bridge

Design detail for the off-LAN monitoring/control pipeline: `web-server`'s hub-side MQTT
bridge (`src/cloud/mqttBridge.js`) and `smartfarm-cloud/` (the self-hosted Next.js
dashboard). See [`architecture.md`](architecture.md) for how this fits into the whole
system. This feature is **additive** — the hub runs identically with or without it.

This **replaces** an earlier AWS-based design (Amplify Gen2: Cognito, DynamoDB, Lambda,
AppSync, IoT Core / Device Shadow) with a fully self-hosted equivalent: a single
Mosquitto broker, exposed publicly through a Cloudflare Tunnel, with no AWS dependency
anywhere in the pipeline.

## Why it exists

`web-server` is deliberately LAN-only: in-memory state, no internet exposure. That's
correct for local irrigation control, but it means the farm can't be viewed or
controlled from off-LAN. This bridge adds that without weakening the local guarantees:
if the broker/dashboard is unreachable, the hub is completely unaffected — no retry
queues, no blocked requests, nothing waits on the cloud.

## Architecture

```
Browser (anywhere) ──HTTPS (Cloudflare Access-gated)──▶ smartfarm.sarachai.com
                                                              │
                                                              ▼
                                              Next.js app (itsci-data.local, Docker)
                                                │                          │
                                                ▼                          ▼
                                          Postgres                  MQTT client
                                     (telemetry history)        (subscribe + publish)
                                                                          │
                                                                          │ wss://mqtt.sarachai.com
                                                                          │ (Cloudflare Tunnel)
                                                                          ▼
                                                              Mosquitto (itsci-data.local)
                                                                          │
                                                                          │ wss:// (same tunnel,
                                                                          │  hub is remote)
                                                                          ▼
                                                              web-server (hub)
                                                              src/cloud/mqttBridge.js
```

`mosquitto`, `postgres`, and the Next.js `dashboard` all run as one Docker Compose stack
on `itsci-data.local` (`smartfarm-cloud/docker-compose.yaml`) — the dashboard talks to
Mosquitto over the internal Docker network (`ws://mosquitto:9001`), not through the
public tunnel; only the hub (a physically separate machine) needs the public
`wss://mqtt.sarachai.com` path. One Cloudflare Tunnel serves both `mqtt.sarachai.com`
(→ Mosquitto's websocket listener) and `smartfarm.sarachai.com` (→ the dashboard
container), gated respectively by MQTT auth+ACLs and Cloudflare Access.

## Wire contract

This is the part that actually has to match between the hub and the dashboard — it's
documented here as the source of truth precisely because it's easy for the two
independently-deployed halves to drift.

**Telemetry, hub → cloud.** Published on every `deviceStore` update (real-time, not
batched) to MQTT topic `farms/{hubId}/telemetry`, QoS 0, one message per device:

```json
{ "hubId": "rasp-01", "device_id": "soil-1", "timestamp": "2026-08-20T12:00:00.000Z", "metrics": { "soil_moisture": 42 } }
```

The dashboard's `lib/mqtt.ts` subscribes `farms/+/telemetry`, writes each message to
Postgres (`TelemetryReading`), and keeps an in-memory "latest per device" cache for
`GET /api/telemetry/latest` to answer without a DB round trip on every poll.

**Commands, cloud → hub.** The dashboard publishes to `farms/{hubId}/command`:

```json
{ "deviceId": "main-pump", "action": { "state": "on" } }
```

The hub executes it (`pumpControl.command()` / `deviceStore.applyCommand()`, same split
as local control) and reports the outcome to `farms/{hubId}/command/result`:

```json
{ "deviceId": "main-pump", "ok": true, "relay_status": "ON", "at": "2026-08-20T12:00:01.000Z" }
```

**No Device Shadow equivalent — fire-and-forget by design.** A command published while
the hub is offline is simply lost, not queued or redelivered. This matches this
project's existing "no retry queue" philosophy (the original AWS design's Device Shadow
gave this for free; rebuilding it against a plain broker was judged not worth the
complexity for a single-hub personal deployment — see the "deliberate v1 limitations"
section below).

## Broker access control

The broker sits behind a public Cloudflare Tunnel, so unlike a LAN-only broker, its
credentials are a real security boundary. Each connecting party gets its own scoped
login rather than sharing one:

| User | Can publish | Can subscribe |
|---|---|---|
| `hub-{hubId}` (e.g. `hub-rasp01`) | `farms/{hubId}/telemetry`, `farms/{hubId}/command/result` | `farms/{hubId}/command` |
| `dashboard-backend` | `farms/+/command` | `farms/+/telemetry`, `farms/+/command/result` |

Configured via Mosquitto's `password_file` + `acl_file` (`smartfarm-cloud/mosquitto/config/`
— `acl` is committed, `passwd` is gitignored since it holds credential hashes). A leaked
`dashboard-backend` credential can't forge telemetry or impersonate a hub; a leaked hub
credential can't issue commands to itself or read other hubs' data.

## Config (hub side)

Env vars for `web-server/src/cloud/mqttBridge.js`, all required together or the bridge
silently stays disabled (missing/invalid → one log line, local operation unaffected):

| Var | Purpose |
|---|---|
| `MQTT_URL` | `wss://mqtt.sarachai.com` — the hub is remote, so it uses the same public WebSocket endpoint the browser dashboard would, rather than a dedicated MQTT(S) port (avoids opening any new inbound port anywhere) |
| `MQTT_USERNAME` / `MQTT_PASSWORD` | This hub's own scoped credentials (`hub-{hubId}`), created on the broker per the table above |
| `MQTT_HUB_ID` | This hub's id (e.g. `rasp-01`) — used in every topic |

## Deliberate v1 limitations

- **Single hub.** Everything is keyed by `hubId` so a second hub is provisioning (new
  broker user + ACL rules + hub env vars), not a schema or code change — but only one is
  deployed today.
- **Pump-only real actuation**, same as the local dashboard — a routing decision in the
  hub's command dispatch, not a schema limitation.
- **No offline command queueing.** A command sent while the hub is disconnected is lost;
  the dashboard has no way to know whether it landed beyond the (also fire-and-forget)
  `command/result` message, if one ever arrives.
- **No telemetry retention/TTL enforcement.** DynamoDB's 90-day TTL was free; Postgres
  needs an explicit scheduled cleanup, not yet implemented — `telemetry_readings` grows
  unbounded until one is added.
- **Auth is entirely edge-gated (Cloudflare Access).** The Next.js app itself has no
  login, session, or user model — anyone who reaches the process (e.g. from inside the
  Docker network) has full access. This is fine because the only public entry point is
  Access-gated `smartfarm.sarachai.com`, but it means there's no defense in depth if
  Access is ever misconfigured.
