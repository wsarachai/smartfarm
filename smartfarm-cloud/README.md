# smartfarm-cloud — remote monitoring & control

The cloud half of the MQTT bridge (see [`../docs/mqtt-cloud-bridge.md`](../docs/mqtt-cloud-bridge.md)
for the full design). A self-hosted Next.js dashboard, Postgres (via Prisma), and a
Mosquitto broker — all Docker Compose, no AWS, no managed backend. Deployed on
`itsci-data.local`, exposed publicly at `smartfarm.sarachai.com` through a Cloudflare
Tunnel, gated by Cloudflare Access (not app-level auth).

```
smartfarm-cloud/
├── docker-compose.yaml     # mosquitto + postgres + dashboard, one stack
├── Dockerfile              # multi-stage: build the Next.js app, then runtime
├── mosquitto/config/       # mosquitto.conf + acl (committed) — passwd is gitignored
├── prisma/schema.prisma    # TelemetryReading model
├── instrumentation.ts      # starts the persistent MQTT client on server boot
├── lib/
│   ├── db.ts                # Prisma client singleton
│   ├── mqtt.ts               # MQTT subscribe (telemetry/results) + publish (commands)
│   └── hubConfig.ts          # HUB_ID / PUMP_DEVICE_ID
└── app/
    ├── layout.tsx           # no auth wrapper — Cloudflare Access gates the edge
    ├── page.tsx              # telemetry table + live readings + pump control
    └── api/
        ├── telemetry/history/route.ts  # GET, Postgres-backed
        ├── telemetry/latest/route.ts   # GET, in-memory cache (polled)
        └── pump/command/route.ts       # POST, publishes to MQTT (fire-and-forget)
```

## Why Postgres/Prisma instead of a JSON file

The rest of this repo (`web-server`) deliberately avoids a database — bounded,
atomically-written JSON files, chosen to minimize SD-card wear on the Jetson/Pi hub. That
constraint doesn't apply here: `itsci-data.local` is a normal PC with a real disk, and
telemetry history needs date-range queries (`WHERE timestamp >= ...`) that a growing JSON
file handles poorly. Prisma was chosen over a raw `pg` client for the type safety +
migration workflow; the schema is small enough that this is a preference call, not a hard
requirement.

## Why no in-app auth

Cognito used to gate the dashboard. Its self-hosted replacement is **Cloudflare Access**
on the `smartfarm.sarachai.com` hostname — configured entirely in the Cloudflare
dashboard, not in this codebase. `app/layout.tsx` renders directly with no login/session
wrapper; the only thing standing between the public internet and this app is Access.

## Setup

**1. Broker credentials.** The `dashboard-backend` MQTT user must already exist on the
broker (see [`../docs/mqtt-cloud-bridge.md`](../docs/mqtt-cloud-bridge.md) for the
`mosquitto_passwd` commands) — `mosquitto/config/passwd` is gitignored, not
regenerated automatically.

**2. Env.**
```
cp .env.local.example .env.local   # for local `npm run dev`
cp .env.example .env               # for `docker compose` variable substitution — DASHBOARD_MQTT_PASSWORD
```

**3. First-time Postgres migration.** With `postgres` running (`docker compose up -d postgres`)
and `DATABASE_URL` pointing at it:
```
npx prisma migrate dev --name init
```
This generates `prisma/migrations/` (committed to git) — `prisma migrate deploy` (run
automatically by the Dockerfile's `CMD`) only *applies* existing migrations, it doesn't
generate new ones, so this step has to happen once from a dev machine before the first
deploy.

**4. Run the whole stack.**
```
docker compose up -d --build
```

## Status

Migrated off AWS Amplify Gen2 (Cognito, DynamoDB, Lambda, AppSync, IoT Core) to this
fully self-hosted stack — see [`../docs/mqtt-cloud-bridge.md`](../docs/mqtt-cloud-bridge.md)
for the wire contract and the "deliberate v1 limitations" this migration carried over
(single hub, pump-only actuation, no offline command queueing) plus the new gaps it
introduced (no telemetry TTL enforcement yet).
