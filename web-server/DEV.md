# Development — testing the pump against real hardware via a Jetson tunnel

The pump firmware runs on a node at **`192.168.0.4:80`** on the *farm* network
(`192.168.0.0/24`, the ESP32 SoftAP). Your dev PC is on a different LAN and
can't reach that network directly. The **Jetson is dual-homed** and bridges the
two:

```
 dev PC                         Jetson                         farm net
┌────────────┐   SSH (LAN)   ┌──────────────┐   Wi-Fi (STA)  ┌──────────┐
│ localhost  │ ────────────▶ │ 192.168.1.124│ ─────────────▶ │ pump     │
│   :8080    │               │ 192.168.0.2  │                │192.168.0.4│
└────────────┘               └──────────────┘                │   :80    │
                                                             └──────────┘
```

An `ssh -L` local forward maps **`localhost:8080` on the PC → `192.168.0.4:80`
(the pump)**. The dev web-server's relay then talks to the pump as if it were
local.

## 1. Open the tunnel

From the PC (keep this terminal open for the session):

```bash
ssh -L 8080:192.168.0.4:80 <user>@192.168.1.124
```

- `<user>` = your Jetson SSH login.
- `8080` = local port on the PC (matches the backend's `PUMP_URL`).
- `192.168.0.4:80` = the pump, resolved from the Jetson's Wi-Fi side.

Verify it's up (should reach the pump's relay API):

```bash
curl http://localhost:8080/api/v1/relay      # -> {"relay_status":"ON"|"OFF"}
```

## 2. Run the dev stack

The pump target is now **server-owned** (persisted in `data/settings.json`, seeded
from the backend's env), so the tunnel is configured on the **backend**, not the
client. Copy the dev env template and set `PUMP_URL` to the tunnel:

```bash
cd web-server && cp .env.example .env    # PUMP_URL=http://localhost:8080 (default)
```

Two terminals (plus the tunnel above):

```bash
# backend (Node relay + APIs) — the pump relay's fetch runs here, on the PC.
# `npm run dev` loads .env via Node's native --env-file (Node >= 20.6).
cd web-server && npm install && npm run dev        # http://localhost:3000

# frontend (Vite dev server, proxies /api -> :3000)
cd web-server/client && npm install && npm run dev
```

## 3. Point the pump at the tunnel

With `PUMP_URL=http://localhost:8080` in `web-server/.env`, the backend seeds
`data/settings.json` with the tunnel URL on first boot, and the relay
(`/api/v1/pump/control`) fetches the pump through it. Nothing to type in the UI.

> **Already have a `data/settings.json`?** The persisted file wins over env, so a
> stale `pump.url` shadows the new dev default. Fix: **Settings → Pump Control
> Settings → Reset Defaults** (writes the env-seeded default back), edit the Pump
> URL field directly, or delete `data/settings.json` and restart. Confirm the
> field reads `http://localhost:8080`.

Now the Dashboard pump card and the Irrigation page drive the **real** pump
through the tunnel: browser → `/api/v1/pump/control` (Vite proxy → Node on the
PC) → the relay reads `pump.url` from settings → `fetch(http://localhost:8080/api/v1/relay)`
→ tunnel → pump.

## Production

`docker-compose.yaml` sets `PUMP_URL` (default `http://192.168.0.5`, the deployed
pump-zone-esp01 node) in the container env, which seeds `data/settings.json` on
first boot. No tunnel, no `.env`, no dev config ships in the container image.
