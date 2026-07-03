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
- `ap-server/` — **PlatformIO / Arduino** firmware for an **ESP-WROOM-32** (`board = esp32dev`) that runs a Wi-Fi **SoftAP with a custom DHCP server** supporting **MAC→IP reservations** managed from a **web UI** and persisted in NVS. Grew out of the AP behavior of `esp-idf-iot/web-server` (ESP-IDF), re-implemented on Arduino to match `esp32cam`'s conventions.
- `sensor-zone/` — **PlatformIO / ESP-IDF** port of `esp-idf-iot/sensor-node` for an **ESP-WROOM-32** (`board = esp32dev`) mapped to a farm zone: reads DHT22 + soil-moisture ADC and POSTs telemetry to the Node web-server. A faithful `framework = espidf` port (not Arduino — the source is 100% IDF-native) using the **pioarduino** platform fork (IDF 5.3.x). Uses a gitignored `include/secrets.h` (+ `secrets.example.h`) to match `esp32cam`/`ap-server` conventions.
- `pump-zone/` — **PlatformIO / ESP-IDF** port of `esp-idf-iot/web-server` for an **ESP-WROOM-32** (`board = esp32dev`) — the **actuator counterpart to `sensor-zone`**: joins the AP as a STA and runs an `esp_http_server` that switches an **irrigation-pump relay** on command, showing status on an **RGB LED**. A deliberately *stripped + modernized* port keeping **only** relay control + LED status (drops the source's SoftAP provisioning, web UI, OTA, RTC/SNTP, sensor cache, and automatic irrigation). `framework = espidf` via the **pioarduino** fork (IDF 5.3.x); gitignored `include/secrets.h` (+ `secrets.example.h`).

## ap-server/

```
ap-server/
├── platformio.ini      # env:esp32dev — espressif32 / arduino, single USB env, port auto-detect
├── README.md           # design decisions + try-it steps
├── .gitignore
├── include/
│   ├── ap_config.h     # AP + addressing + reservation tunables (committed, not secret)
│   ├── reservations.h  # NVS-backed MAC→IP store (API)
│   └── dhcp_server.h   # custom DHCP server (API)
└── src/
    ├── main.cpp        # SoftAP bring-up + web UI (dashboard, /edit, CRUD endpoints)
    ├── reservations.cpp# reservation store (Preferences/NVS) + parseMac/macToStr
    └── dhcp_server.cpp # DISCOVER/OFFER/REQUEST/ACK/NAK/RELEASE on UDP/67
```

- **AP-only** mode (no STA uplink), WPA2, standalone network (SSID `MJU-SmartFarm-AP-II`, password `password` — **change `AP_PASSWORD`**, `192.168.1.1`, ch 1, **max 10 clients**), deliberately distinct from the ESP-IDF reference AP (`MJU-SmartFarm-AP` @ `192.168.0.1`). All tunables in committed `include/ap_config.h`; no `secrets.h`.
- **Why a custom DHCP server:** the built-in ESP DHCP server (`dhcpserver.h`) exposes only a pool *range* (`dhcps_lease_t`) — **no MAC-based reservation API**, and its source is precompiled into the framework's lwIP lib so it can't be patched from PlatformIO. So `main.cpp` calls `esp_netif_dhcps_stop("WIFI_AP_DEF")` and `dhcp_server.cpp` runs our own on UDP/67, **polled from `loop()`** (single-threaded with `WebServer`, so the reservation table needs no locking).
- **Addressing:** `.1` = AP; **`.2`–`.99` reserved** (MAC→IP, the "server group"); **`.100`–`.109` dynamic** (`DHCP_POOL_FIRST_HOST` .. `+AP_MAX_CONNECTIONS-1`, guarded by `static_assert`). Reserved MAC → fixed IP; unknown MAC → dynamic lease (RAM table, reused per MAC, 2 h lease `DHCP_LEASE_SECS`).
- **Reservations** persist in NVS via `Preferences` (namespace `apres`, one blob of `{mac[6],octet,label[≤24]}`, cap `MAX_RESERVATIONS`=32); loaded in `reservations::begin()` at boot. Web UI: `/` dashboard (auto-refresh; connected clients w/ one-click **Reserve** prefilling MAC, + reservations table w/ **Delete**), `/edit` form (no refresh, so it isn't wiped mid-typing), `POST /api/reservations` (upsert, validates MAC + IP∈`.2`–`.99` + uniqueness), `POST /api/reservations/delete`. **No auth** — WPA2 is the gate. Changes apply on the device's **next DHCP renewal/reconnect**.
- Commands: `cd ap-server && pio run` / `pio run -t upload` / `pio device monitor` (115200). **Compiles clean on arduino-esp32 2.0.17 (esp32dev)** but the hand-rolled DHCP server is **not yet hardware-tested** — during first bring-up keep one client on a static `192.168.1.50` as a lifeline in case DHCP misbehaves.

## sensor-zone/

```
sensor-zone/
├── platformio.ini        # env:esp32dev (espidf) + env:native (unity tests)
├── sdkconfig.defaults     # minimal parity keys (esp32, single-app, 4MB, 160MHz, 100Hz, INFO)
├── README.md
├── .gitignore
├── docs/PARITY.md         # legacy-vs-new telemetry parity procedure
├── include/
│   ├── secrets.h          # gitignored — SSID/password/host/port (current topology)
│   └── secrets.example.h  # committed placeholder template
├── src/                   # flattened verbatim port of esp-idf-iot/sensor-node/main/
│   ├── main.c  main.h  task_settings.h
│   ├── CMakeLists.txt     # single idf_component_register (+ "../include" for secrets.h)
│   ├── actuators/rgb_led.{c,h}
│   ├── network/wifi_sta.{c,h}   # config #defines moved out to include/secrets.h
│   ├── sensors/{dht22,soil_moisture_adc,sensor_task}.{c,h}
│   └── http/http_client.{c,h}
├── test/                  # host-side Unity tests (pio test -e native)
│   ├── test_soil_map/     # voltage→percent mapping + clamp
│   ├── test_dht_checksum/ # DHT22 decode + checksum accept/reject
│   └── test_json_body/    # telemetry body shape == server contract
└── tools/parity_diff.py   # diffs sensor readings from two serial-log captures
```

- **Why `framework = espidf` (not Arduino):** the legacy source is 100% ESP-IDF-native (`esp_adc` oneshot+cali, `esp_http_client`, `esp_wifi`/`esp_netif`/`esp_event`, `nvs_flash`, IDF `driver/ledc`, `esp_efuse_mac`, `esp_rom_delay_us`). An Arduino port would be a rewrite of every module and would *break* signal parity, so the port keeps the C logic byte-for-byte and only changes the build wrapper + telemetry contract.
- **Why the pioarduino platform fork:** the code needs **IDF ≥ 5.2** (`ADC_ATTEN_DB_12` + `esp_adc/adc_oneshot`), which mainline `platformio/espressif32` doesn't ship. `platformio.ini` pins `https://github.com/pioarduino/platform-espressif32.git#53.03.13` (Arduino core 3.1.3 / **IDF 5.3.2**), matching the legacy IDF 5.3.1.
- **Layout:** the legacy `main/` is a *single* IDF component with subfolders, so it's flattened verbatim into `src/` with one adapted `src/CMakeLists.txt` (no `lib/` / `components/` split).
- **GPIO map (preserved exactly from the legacy build):** DHT22 data `GPIO32` (bit-banged); soil moisture `ADC1_CH6` = `GPIO34` @ `ADC_ATTEN_DB_12` (DRY 2800 mV → 0%, WET 1200 mV → 100%); RGB status LED via LEDC on `GPIO25/26/27` (active-high, 5 kHz, 8-bit, ch 0/1/2, timer 0); onboard status LED `GPIO2` (active-low, 500 ms blink). No I2C/SPI in use.
- **Telemetry contract changed on purpose:** the legacy firmware POSTed a *flat* body to the old ESP-IDF web-server (`192.168.0.1:80/sensor-update`). This port targets the **Node web-server** (`192.168.1.2:3000/api/v1/telemetry`), which **requires** a `metrics{}` object (else HTTP 400), so `http_client.c` nests the readings: `{"device_id":"…","metrics":{"temperature":…,"humidity":…,"soil_moisture":…}}` (timestamp omitted — no RTC; the server stamps it). Because the HTTP body deliberately differs from the legacy binary, **parity is verified on the physical-pin readings** (serial-log diff via `tools/parity_diff.py`), not on the wire payload.
- Commands: `cd sensor-zone && cp include/secrets.example.h include/secrets.h` (fill creds) → `pio run` / `pio run -t upload` / `pio device monitor` (115200) / `pio test -e native`. **Not yet compiled or hardware-verified** — the first `pio run` pulls the pioarduino platform + IDF 5.3.2 (slow, one-time); confirm `ADC_ATTEN_DB_12` resolves before trusting it.

## pump-zone/

```
pump-zone/
├── platformio.ini        # env:esp32dev (espidf) via pioarduino IDF 5.3.x, single USB env
├── sdkconfig.defaults     # generic parity keys (esp32, single-app, 4MB, 160MHz, 100Hz, INFO)
├── CMakeLists.txt         # top-level IDF project file
├── README.md
├── .gitignore
├── include/
│   ├── pump_config.h     # COMMITTED hw contract: RELAY_GPIO/ACTIVE_LEVEL, RGB pins, PUMP_HTTP_PORT
│   ├── secrets.h         # gitignored — WIFI_STA_AP_SSID/PASSWORD, DEVICE_ID
│   └── secrets.example.h # committed placeholder template
└── src/                  # stripped/modernized subset of esp-idf-iot/web-server/main/
    ├── main.c  main.h  task_settings.h
    ├── CMakeLists.txt    # idf_component_register (+ "../include", REQUIRES json for cJSON)
    ├── actuators/relay.{c,h}   # single relay, on/off (manual_override + auto dropped)
    ├── actuators/rgb_led.{c,h} # LEDC RGB, color-by-id (pins from pump_config.h)
    ├── network/wifi_sta.{c,h}  # STA-only, reused verbatim from sensor-zone
    └── http/http_server.{c,h}, http_server_relay.c  # relay-only server + JSON handler
```

- **Role in the topology:** unlike `sensor-zone` (an HTTP *client* that POSTs telemetry up), pump-zone is an HTTP *server*. It joins `MJU-SmartFarm-AP-II` as a STA (creds from `secrets.h`), takes a **DHCP** lease, and listens on `PUMP_HTTP_PORT` (80). Give it a stable address by **reserving its MAC** in `ap-server`'s web UI (the `.2`–`.99` "server group"). The Node hub or a browser POSTs pump commands **to** it.
- **Why `framework = espidf` + pioarduino:** the source `esp-idf-iot/web-server` is 100% IDF-native (`esp_http_server`, `esp_wifi`/`esp_netif`/`esp_event`, `driver/ledc`, `nvs_flash`, cJSON). Same platform pin as the siblings (`#53.03.13` = Arduino core 3.1.3 / IDF 5.3.2).
- **Deliberately stripped:** kept only `relay.c`, `rgb_led.c`, the HTTP server core + relay handler, and `main.c`. **Dropped** from the source: SoftAP + provisioning web UI + embedded assets, `app_nvs`, `irrigation_ctrl` (automatic irrigation), `sensor_cache`, `water_config`, `time_sync`/`rtc_ds3231`, OTA, and the monitor/status/wifi HTTP handlers. WiFi is `sensor-zone`'s STA-only `wifi_sta.c` reused verbatim (matching its `APP_MSG_WIFI_*` event contract), not the source's dual AP+STA `wifi_app.c`.
- **HTTP API (modernized to a JSON body):** `POST /api/v1/relay` with `{"state":"on"|"off"}` → `{"relay_status":"ON"|"OFF"}`; `GET /api/v1/relay` returns the same. Parsed with the IDF-bundled **cJSON** (`REQUIRES json`, no new PlatformIO deps). This intentionally replaces the source's header-based `/relayControl.json` contract and drops the `manual_override`/`auto` concept (there's no auto controller left to hand back to).
- **GPIO map:** relay (pump) on `GPIO23` **active-low** (moved off the `GPIO2` strapping/onboard-LED pin; polarity + pin in `include/pump_config.h`); RGB status LED via LEDC on `GPIO25/26/27` active-high (same wiring as `sensor-zone`). LED status: **BLUE** boot/connecting → **GREEN** connected + server up → **RED** WiFi disconnected → **MAGENTA** while the pump is ON (overrides the status color, restored when it stops).
- Commands: `cd pump-zone && cp include/secrets.example.h include/secrets.h` (fill creds) → `pio run` / `pio run -t upload` / `pio device monitor` (115200). Then `curl -X POST http://<ip>/api/v1/relay -d '{"state":"on"}'`. **Not yet compiled or hardware-verified** — the first `pio run` pulls the pioarduino platform + IDF 5.3.2 (slow, one-time). No native test env in v1.
