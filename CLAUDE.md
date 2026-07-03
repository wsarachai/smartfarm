# Role
You are a Principal IoT Solutions Architect and Lead Full-Stack Developer specializing in Smart Agriculture (AgTech) infrastructure, multi-architecture Docker containerization for resource-constrained Edge hardware (NVIDIA Jetson Nano running Ubuntu 18.04), and modular React-Redux control dashboards.

# Context
I am building a foundational, centralized **Smart Farm Web Control Center** to ingest telemetry data from field sensors (soil moisture, temperature, etc.) and send remote commands down to actuators (valves, pumps, switches). 

The target deployment host is an older Jetson Nano (Ubuntu 18.04 LTS). Because system memory and CPU are highly constrained on this machine—and must be preserved for potentially heavy edge automation or local AI models—I cannot run an independent React development server at runtime. 

Instead, the entire system must be containerized using Docker, with the React frontend compiled into static assets ahead of time and served directly via a unified Node.js/Express server on a single network port. This initial version must act as a clean, highly generic blueprint that allows me to plug in new device types and features later.

# Architectural & Container Requirements

### 1. Multi-Stage Docker & Compose Layout
* **Dockerfile:** Create an optimized multi-stage build targeting an ARM64/Jetson Nano friendly runtime base (like `node:16-alpine` or `node:18-slim` compatible with Ubuntu 18.04 glibc baselines).
  * *Stage 1 (Build):* Installs frontend tools and compiles the React application into a static production folder (`dist` or `build`).
  * *Stage 2 (Runtime):* Drops heavy build tools, copies over the static frontend assets and backend code, installs production-only Node dependencies, and exposes a single unified web port.
* **docker-compose.yml:** Include a basic compose file configured for low resource environments, managing auto-restarts and simple port exposure.

### 2. Extensible Node.js Core Backend
* **Data Ingestion API:** Create a lightweight POST endpoint (`/api/v1/telemetry`) to process generic JSON payloads (e.g., `device_id`, `timestamp`, `metrics: {}`) in-memory to prevent hammering the Jetson's SD card storage.
* **Device Command API:** Create a POST endpoint (`/api/v1/control`) to receive a target device ID and an action payload, ready for distribution to field nodes.
* **Unified Hosting:** Configure `express.static()` to natively host the pre-built React asset directory, gracefully mapping SPA routing fallbacks to `index.html`.

### 3. Modular React & Redux Toolkit UI
* **Generic Dashboard Grid:** Provide a clean, grid-based dashboard that dynamically displays a collection of "Device Cards."
* **Scalable Redux Structure:** Design a generic `devicesSlice` that stores field devices as objects in a flexible dictionary. The state must dynamically update whether an incoming metric is recorded or an actuator switch is flipped.
* Use lightweight polling mechanisms (like standard React intervals or RTK Query fixed intervals) to pull field changes safely without causing memory leaks or UI freeze on the browser side.

# Constraints & Design Principles
* **Hardware Agnostic Data Schemas:** Treat field units as generic definitions (e.g., "Device_01" with a dictionary of reading keys) so adding completely new sensor types later requires zero code changes.
* **Zero Runtime Overhead:** Absolutely no developer tools, live-reload watchers, or independent dev servers are permitted to run in the background at production runtime.

# Expected Output Format
Please provide:
1. **Infrastructure Files:** The complete production `Dockerfile` and `docker-compose.yml`.
2. **Project Directory Layout:** A clear folder tree showing how the frontend, backend, and Docker configs are mapped.
3. **The Server Code (`server.js`):** The clean Express application handling state routing, static delivery, and API endpoints.
4. **Frontend Architecture:** The Redux Toolkit state slice and a reusable React component that cleanly switches its rendering mode depending on whether a device is defined as a sensor or an actuator.

# Current Implementation

The scaffold described above has been built out under `web-server/`:

```
web-server/
├── Dockerfile              # multi-stage: build client, then slim runtime
├── docker-compose.yaml
├── package.json            # backend deps (express only)
├── server.js               # Express entry point
├── src/
│   ├── routes/
│   │   ├── telemetry.js    # POST /api/v1/telemetry
│   │   ├── control.js      # POST /api/v1/control
│   │   ├── devices.js      # GET  /api/v1/devices (added so the
│   │   │                   #   dashboard has something to poll)
│   │   ├── health.js       # GET  /api/v1/health (liveness for
│   │   │                   #   Docker healthcheck)
│   │   └── camera.js       # ESP32-CAM: POST /frame ingest, GET
│   │                       #   /frame.jpg, /stream (MJPEG), /status
│   └── store/
│       ├── deviceStore.js  # in-memory Map keyed by device_id
│       └── frameStore.js   # in-memory single-slot latest JPEG frame
└── client/                 # Vite + React + Redux Toolkit frontend
    ├── package.json
    ├── vite.config.js
    └── src/
        ├── app/store.js
        ├── features/devices/
        │   ├── devicesApi.js     # RTK Query: getDevices (polled), sendCommand
        │   ├── devicesSlice.js   # normalized dict keyed by device_id
        │   ├── Dashboard.jsx     # grid of DeviceCards, polls every 5s
        │   └── DeviceCard.jsx    # renders sensor readout vs actuator controls
        └── features/camera/
            ├── cameraApi.js      # RTK Query: getCameraStatus (polled)
            ├── cameraSlice.js    # camera online/stale + last-frame meta
            └── CameraCard.jsx    # MJPEG <img> + live/stale/offline badge
```

## Commands

- Backend: `npm install && npm start` (serves on `PORT`, default 3000).
- Frontend dev: `cd client && npm install && npm run dev` (Vite dev server, proxies `/api` to `localhost:3000`).
- Frontend production build: `cd client && npm run build` → outputs `client/dist`, which `server.js` serves via `express.static`.
- Full container: `docker compose up --build` from `web-server/`.

## Notes

- `GET /api/v1/devices` was added beyond the original two endpoints since the dashboard needs a way to fetch current state to poll.
- `GET /api/v1/health` returns `{ status, uptime, deviceCount, timestamp }` and backs the `docker-compose.yaml` healthcheck. The healthcheck probes it with `node -e` (via `http.get`) rather than `curl`/`wget`, keeping the `node:18-alpine` image dependency-free.
- **ESP32-CAM camera feed** (`src/routes/camera.js` + `src/store/frameStore.js`): the camera firmware's PUSH mode POSTs raw `image/jpeg` to `POST /api/v1/camera/frame`; the server keeps only the **latest** frame in a single-slot in-memory buffer (no SD writes) and exposes `GET /frame.jpg` (snapshot), `GET /stream` (multipart/x-mixed-replace MJPEG relay — all viewers share the one buffer), and `GET /status`. Uses the built-in `express.raw()` — no new npm deps. Tunables: `CAMERA_MAX_FRAME_BYTES`, `CAMERA_STALE_MS`. On the camera side set `PUSH_ENABLED 1` and `PUSH_URL "http://<jetson-ip>:3000/api/v1/camera/frame"` in `include/secrets.h`.
- Device state is in-memory only (a `Map` in `deviceStore.js`) — it resets on restart, by design, to avoid SD card wear on the Jetson.
- Not yet verified end-to-end (no `node`/`npm`/network access in the sandbox this was built in) — run the commands above on the target machine before trusting it.

# ESP32 Firmware Projects

The repo also contains firmware for the field devices that talk to the web-server:

- `esp-idf-iot/` — the reference **ESP-IDF** workspace (`web-server/` SoftAP+HTTP+OTA control server, `sensor-node/`, and `examples/`). GPL-3.0. The canonical smart-farm firmware; the projects below borrow its conventions.
- `esp32cam/` — **PlatformIO / Arduino** firmware for the AI-Thinker ESP32-CAM that pushes JPEG frames to the web-server's `/api/v1/camera/frame` (see the camera notes above). Uses a gitignored `include/secrets.h` (+ `secrets.example.h`) for WiFi/OTA creds.
- `ap-server/` — **PlatformIO / Arduino** firmware for an **ESP-WROOM-32** (`board = esp32dev`) that runs a Wi-Fi **SoftAP + DHCP server** with a live status page. It re-implements the AP + DHCP behavior of `esp-idf-iot/web-server` (which is ESP-IDF) on the Arduino framework, to match `esp32cam`'s conventions.

## ap-server/

```
ap-server/
├── platformio.ini      # env:esp32dev — espressif32 / arduino, single USB env, port auto-detect
├── README.md           # design decisions + try-it steps
├── .gitignore
├── include/
│   └── ap_config.h     # AP SSID/pass/IP/channel/max clients (committed, not secret)
└── src/
    └── main.cpp        # WiFi.softAPConfig + WiFi.softAP + WebServer status page
```

- **AP-only** mode (no STA uplink), WPA2. `WiFi.softAPConfig()` pins the AP IP to `192.168.0.1/24`; the Arduino-ESP32 core auto-starts a DHCP server that leases `192.168.0.2`+ to clients.
- Defaults in `include/ap_config.h` **mirror** `esp-idf-iot/web-server/main/network/wifi_app.h` (SSID `MJU-SmartFarm-AP`, password `password`, `192.168.0.1`, channel 1, max 5 clients) so existing `sensor-node` firmware can associate unchanged. **The password is `password` — change `AP_PASSWORD` before real deployment.** Config lives in a committed header (no `secrets.h`) because a WPA2 PSK is shared with every client anyway.
- The status page at `http://192.168.0.1/` (built-in `WebServer.h`, zero extra `lib_deps`) joins the WiFi driver's station list (`esp_wifi_ap_get_sta_list`) with the netif DHCP lease table (`esp_netif_get_sta_list`) to show each client's **MAC + leased IP**, meta-refreshing every 3s. Any path returns the status page.
- Scope is deliberately just AP + DHCP + status page — no HTTP control UI, STA config, relay, OTA, or NVS (unlike the ESP-IDF reference).
- Commands: `cd ap-server && pio run` (build), `pio run -t upload` (flash via USB), `pio device monitor` (serial @ 115200). Not yet compiled/flashed — no `pio` in the sandbox this was built in; build on the target machine before trusting it.
