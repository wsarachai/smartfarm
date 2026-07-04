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
- `8080` = local port on the PC (matches `VITE_PUMP_URL`).
- `192.168.0.4:80` = the pump, resolved from the Jetson's Wi-Fi side.

Verify it's up (should reach the pump's relay API):

```bash
curl http://localhost:8080/api/v1/relay      # -> {"relay_status":"ON"|"OFF"}
```

## 2. Run the dev stack

Two terminals (plus the tunnel above):

```bash
# backend (Node relay + APIs) — the pump relay's fetch runs here, on the PC
cd web-server && npm install && npm start        # http://localhost:3000

# frontend (Vite dev server, proxies /api -> :3000)
cd web-server/client && npm install && npm run dev
```

## 3. Point the pump at the tunnel

`client/.env.development` sets:

```
VITE_PUMP_URL=http://localhost:8080
```

`vite dev` loads this so the default pump URL (`DEFAULT_PUMP_SETTINGS.url`)
becomes the tunnel automatically. Nothing to type.

> **Stale localStorage:** pump settings persist in the browser. If you'd
> previously saved `http://192.168.0.4`, that saved value shadows the new dev
> default. Fix: **Settings → Pump Control Settings → Reset Defaults**, or use a
> fresh browser profile. Confirm the field reads `http://localhost:8080`.

Now the Dashboard pump card and the Irrigation page drive the **real** pump
through the tunnel: browser → `/api/v1/pump/control` (Vite proxy → Node on the
PC) → `fetch(http://localhost:8080/api/v1/relay)` → tunnel → pump.

## Production

`vite build` does **not** load `.env.development`, so `VITE_PUMP_URL` is unset
and `DEFAULT_PUMP_SETTINGS.url` falls back to the real pump IP `http://192.168.0.4`.
No tunnel, no dev config ships in the container image.
