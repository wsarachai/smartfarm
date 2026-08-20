# Smart Farm — System Architecture

This is the authoritative architecture reference for the whole repository: what each
component is, how data and commands move between them, and the design principles that
constrain how new components get added. `README.md` has the step-by-step deployment
guide; this document explains *why* the system is shaped the way it is. `design.md` at
the repo root is an early brainstorm that predates the actual implementation (it
mentions FastAPI/SQLite, for example, neither of which was used) — treat it as
historical, not current.

## 1. Layers

The system is three layers, each independently deployable, each degrading gracefully if
a layer above or below it is unreachable:

```
                                   ┌───────────────────────────┐
                                   │   Users (browser / phone)  │
                                   └──────────────┬──────────────┘
                                                  │
                    ┌─────────────────────────────┼─────────────────────────────┐
                    │ off-LAN, over the internet   │  on-LAN                     │
                    ▼                              │                             │
   ┌────────────────────────────────┐              │                             │
   │ CLOUD (self-hosted,             │              │                             │
   │ smartfarm-cloud/) — optional,   │              │                             │
   │ additive — the farm runs fully  │              │                             │
   │ without it                      │              │                             │
   │                                  │              │                             │
   │  Next.js dashboard (Docker on   │              │                             │
   │  itsci-data.local, Cloudflare   │              │                             │
   │  Access-gated)                  │              │                             │
   │         │                        │              │                             │
   │         ▼                        │              │                             │
   │  Postgres (telemetry history) ◀──MQTT client     │                             │
   │                                  (subscribe/pub) │                             │
   │                                  — full detail   │                             │
   │                                  in mqtt-cloud-  │                             │
   │                                  bridge.md        │                             │
   │  Mosquitto broker (Docker,      │              │                             │
   │  same host, Cloudflare Tunnel)  │              │                             │
   └────────────────┬─────────────────┘              │                             │
                    │ MQTT over wss:// (Cloudflare    │                             │
                    │ Tunnel — hub is remote)          │                             │
                    ▼                                  ▼                             ▼
   ┌──────────────────────────────────────────────────────────────────────────────────┐
   │ LOCAL HUB — NVIDIA Jetson Nano / Raspberry Pi 3B (web-server/, smartfarm-ai/,      │
   │ edge-ctrl/) — the system's actual brain; everything below this line is optional    │
   │                                                                                     │
   │  ┌───────────────────────────────┐   ┌──────────────────┐   ┌───────────────────┐  │
   │  │ web-server (Docker, :3000)     │──▶│ smartfarm-ai      │   │ edge-ctrl          │  │
   │  │  - Express API + static React  │◀──│ (Docker, :8000)   │   │ (native C++17      │  │
   │  │  - telemetry/control REST      │   │  - water stress    │   │  daemon, no        │  │
   │  │  - irrigation scheduler        │   │  - canopy coverage  │   │  Docker)           │  │
   │  │  - camera MJPEG proxy          │   │  - disease (CNN)    │   │  - enclosure fan   │  │
   │  │  - src/cloud/awsIotBridge.js   │   └──────────────────┘   │  - thermal governor │  │
   │  └───────────────┬─────────────────┘                         │  - DS3231 RTC sync  │  │
   └──────────────────┼───────────────────────────────────────────┴───────────────────┘  │
                      │ HTTP, local network only
        ┌─────────────┼──────────────────────────┬───────────────────────┐
        ▼             ▼                          ▼                       ▼
  ┌───────────┐ ┌───────────┐            ┌───────────────┐      ┌──────────────────┐
  │ sensor-   │ │ pump-zone /│            │ esp32cam       │      │ ap-server         │
  │ zone      │ │ pump-zone- │            │ (MJPEG source, │      │ (SoftAP + DHCP    │
  │ (DHT22 +  │ │ esp01      │            │  ~1 frame/10s  │      │  w/ MAC→IP        │
  │  soil ADC)│ │ (relay)    │            │  push + live   │      │  reservations)    │
  └───────────┘ └───────────┘            │  :81 pull)      │      └──────────────────┘
                                          └───────────────┘
```

- **Field devices** only sense or actuate — no local intelligence, no scheduling, no state beyond what's needed to reconnect Wi-Fi.
- **The local hub** owns everything: ingestion, state, scheduling, safety, the dashboard, and *calls out* to AI for decisions. It runs fully standalone; nothing above this line is required.
- **The cloud** is a bolt-on for off-LAN visibility/control, fully self-hosted (no AWS) — see [`mqtt-cloud-bridge.md`](mqtt-cloud-bridge.md) for its own design detail. If the broker/dashboard is unreachable, the hub is unaffected.

## 2. Components

| Component | Stack | Role | Runs on |
|---|---|---|---|
| [`web-server/`](../web-server) | Node.js (Express) + React (Redux Toolkit) | Central control server: telemetry ingestion, device/actuator control, irrigation scheduling, camera proxy, settings, cloud bridge | Hub (Docker) |
| [`smartfarm-ai/`](../smartfarm-ai) | Python 3 (stdlib `http.server`, PyTorch/TFLite, PIL+numpy) | AI decision microservice — water stress (rule-based), canopy coverage (HSV thresholding), disease detection (PlantVillage CNN) | Hub (Docker) |
| [`edge-ctrl/`](../edge-ctrl) | C++17 native daemon + Python tooling | Enclosure cooling fan control, thermal safety governor, DS3231 RTC sync | Hub (native systemd service, no Docker) |
| [`smartfarm-cloud/`](../smartfarm-cloud) | Next.js + Postgres (Prisma) + Mosquitto, Docker | Off-LAN dashboard: telemetry history + remote pump control, self-hosted | `itsci-data.local` (Docker) |
| [`sensor-zone/`](../sensor-zone) | ESP-IDF (C), ESP-WROOM-32 | Reads DHT22 (temp/humidity) + soil-moisture ADC, POSTs telemetry | Field |
| [`pump-zone/`](../pump-zone) | ESP-IDF (C), ESP-WROOM-32 | Relay-driven pump actuator, HTTP server, RGB status LED | Field |
| [`pump-zone-esp01/`](../pump-zone-esp01) | Arduino (C++), ESP-01/01S | Cheaper drop-in replacement for `pump-zone` — same `/api/v1/relay` contract, adds a local dead-man safety cutoff + OTA | Field |
| [`esp32cam/`](../esp32cam) | Arduino (C++), AI-Thinker ESP32-CAM | Pushes JPEG frames to the hub; also serves its own high-fps `:81` MJPEG stream | Field |
| [`ap-server/`](../ap-server) | Arduino (C++), ESP-WROOM-32 | Standalone Wi-Fi SoftAP with a custom DHCP server (MAC→IP reservations via web UI) for the field network | Field (network infra) |

`esp-idf-iot/`, referenced as a "reference workspace" in some older notes, does not
exist in this checkout — don't rely on that reference.

### 2.1 Self-hosted modules (inside `smartfarm-cloud/`)

The single `smartfarm-cloud` row above is one Docker Compose stack of three services on
`itsci-data.local`, replacing what used to be a multi-service AWS Amplify Gen2 app
(Cognito, AppSync, 2 Lambdas, DynamoDB, IoT Core, Amplify Hosting). Full wire contract
and ACL detail in [`mqtt-cloud-bridge.md`](mqtt-cloud-bridge.md).

| Service | Resource | Role | Defined in |
|---|---|---|---|
| **dashboard** | Next.js app (Docker, `next start`) | Serves the UI + API routes; runs a persistent in-process MQTT client (via `instrumentation.ts`) instead of a Lambda-per-request model | [`../smartfarm-cloud/Dockerfile`](../smartfarm-cloud/Dockerfile) |
| **dashboard** | `app/api/telemetry/history`, `app/api/telemetry/latest`, `app/api/pump/command` | Plain Next.js route handlers — no GraphQL/AppSync layer | [`../smartfarm-cloud/app/api`](../smartfarm-cloud/app/api) |
| **postgres** | `TelemetryReading` table (Prisma) | Telemetry store: `hubId`+`timestamp` indexed, no TTL enforcement yet (a gap vs. the old DynamoDB TTL — see mqtt-cloud-bridge.md) | [`../smartfarm-cloud/prisma/schema.prisma`](../smartfarm-cloud/prisma/schema.prisma) |
| **mosquitto** | Broker, `password_file` + `acl_file` | Replaces AWS IoT Core entirely — plain MQTT auth/ACLs instead of X.509 + IAM policies, no Device Shadow (commands are fire-and-forget) | [`../smartfarm-cloud/mosquitto/config`](../smartfarm-cloud/mosquitto/config) |

Auth is **not** one of these services — it's Cloudflare Access, gating
`smartfarm.sarachai.com` entirely at the edge, outside this repo. The app itself has no
login/session/user model.

## 3. Data flows

**Telemetry ingestion (field → hub):** `sensor-zone` POSTs `{device_id, timestamp,
metrics}` to `web-server`'s `POST /api/v1/telemetry`. `deviceStore.js` (in-memory `Map`,
resets on restart by design — no SD-card wear) upserts it and fires a single-slot
listener (`setTelemetryListener`), currently chained by two consumers: `telemetryStore.js`
(7-day JSON history for trend charts) and, if configured, `src/cloud/awsIotBridge.js`
(publishes to AWS IoT Core in real time).

**Command / control (dashboard or cloud → field):** The pump has the only real
hardware-relay path today: `POST /api/v1/pump/control` → `pumpControl.command()`, which
relays to the pump node's `POST /api/v1/relay`, arms a server-authoritative auto-off
timer, and logs every attempt (`pumpLog.js`, persisted). The generic `POST
/api/v1/control` endpoint exists for any other `device_id` but is currently
state-only (`deviceStore.applyCommand()`) — it updates what the dashboard displays, not
real hardware, until that device type gets its own relay route the way the pump did.
Cloud-originated commands (via the self-hosted MQTT bridge) are routed through this same
split in `mqttBridge.js`, not a parallel path.

**Camera:** `esp32cam` pushes JPEG frames (~1/10s) to `POST /api/v1/camera/frame` (RAM
ring buffer, single slot, no SD writes); the hub serves `/frame.jpg` (snapshot,
ETag-cached), `/stream` (MJPEG relay of pushed frames), and `/live` (a same-origin proxy
that pulls the camera's own high-fps `:81` stream once and fans it out to every viewer,
so off-camera-subnet browsers still see live video).

**AI insights:** `web-server` is the *orchestrator* (aggregates fresh telemetry / grabs
the latest frame, applies smoothing, caches, persists history, degrades); `smartfarm-ai`
is the *decision engine* (pure inference, stateless). Three insights exist: water
stress (rule-based thresholds), canopy coverage (HSV thresholding), and disease
detection (PlantVillage CNN, on-demand only). All are advisory-only — none of them
actuate the pump directly.

**Irrigation scheduling:** `irrigationScheduler.js` ticks every ~20s, checks
`settings.json`'s schedule entries against the current time in a configured IANA
timezone, and fires `pumpControl.command()` when a moisture-guarded, deduped match
hits. `irrigation.auto` is a server-global switch — in AUTO, manual `ON` (from the
dashboard *or* the cloud) is refused with `409`; manual `OFF` always works as an
emergency stop.

**Cloud sync (hub ⇄ self-hosted MQTT broker, additive):** see
[`mqtt-cloud-bridge.md`](mqtt-cloud-bridge.md) for the full design. In short: real-time
telemetry publish up, fire-and-forget command publish down (no offline queueing),
everything keyed by `hubId` for a currently-single hub.

## 4. Design principles

These are load-bearing constraints, not preferences — violating them breaks the
platform's fit for its actual deployment target (an old, resource-constrained SBC in a
field, potentially offline):

- **Hardware-agnostic schemas.** Field units are generic `{device_id, metrics: {}}` —
  adding a new sensor type needs zero server code changes.
- **No SD-card wear.** Device state and camera frames are in-memory only; the only
  things persisted to disk are bounded, atomically-written JSON files (`settings.json`,
  `pump-log.json`, telemetry/insight history), explicitly chosen over a database.
- **Zero runtime overhead.** No dev servers, watchers, or live-reload in production —
  the React frontend is pre-built and served as static assets from the same Express
  process that serves the API.
- **Graceful degradation everywhere.** AI unreachable → cached last result + "AI
  offline" flag, never a crash or a stale-looking success. Cloud unreachable → local
  hub is completely unaffected; this is why the cloud bridge is additive rather than a
  dependency.
- **One authoritative actuation path per device.** All pump commands — manual,
  scheduled, auto-off, or cloud — go through `pumpControl.command()`, so safety logic
  (auto-off timer, AUTO-mode guard, logging) only has to be written and tested once.

## 5. Related docs

- [`mqtt-cloud-bridge.md`](mqtt-cloud-bridge.md) — cloud pipeline design detail (this repo's docs/ folder)
- [`../README.md`](../README.md) — end-to-end deployment/setup guide
- [`../web-server/DEV.md`](../web-server/DEV.md) — Express routes, Redux slices, pump scheduling
- [`../web-server/docs/ai-features-roadmap.md`](../web-server/docs/ai-features-roadmap.md) — AI feature roadmap
- [`../smartfarm-ai/README.md`](../smartfarm-ai/README.md) — AI service API reference
- [`../edge-ctrl/docs/hardware-spec.md`](../edge-ctrl/docs/hardware-spec.md) — pinout matrix, Linux driver details
- [`../smartfarm-cloud/README.md`](../smartfarm-cloud/README.md) — cloud project setup (Docker Compose, Prisma migrations)
