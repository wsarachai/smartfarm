# Role
You are a Principal IoT Solutions Architect and Lead Full-Stack Developer specializing in Smart Agriculture (AgTech) infrastructure, multi-architecture Docker containerization for resource-constrained Edge hardware (NVIDIA Jetson Nano running Ubuntu 18.04), and modular React-Redux control dashboards.

# Context
I am building a foundational, centralized **Smart Farm Web Control Center** to ingest telemetry data from field sensors (soil moisture, temperature, etc.) and send remote commands down to actuators (valves, pumps, switches). 

The target deployment host is an older Jetson Nano (Ubuntu 18.04 LTS). Because system memory and CPU are highly constrained on this machine—and must be preserved for potentially heavy edge automation or local AI models—I cannot run an independent React development server at runtime. 

Instead, the entire system must be containerized using Docker, with the React frontend compiled into static assets ahead of time and served directly via a unified Node.js/Express server on a single network port. This initial version must act as a clean, highly generic blueprint that allows me to plug in new device types and features later.

# Constraints & Design Principles
* **Hardware Agnostic Data Schemas:** Treat field units as generic definitions (e.g., "Device_01" with a dictionary of reading keys) so adding completely new sensor types later requires zero code changes.
* **Zero Runtime Overhead:** Absolutely no developer tools, live-reload watchers, or independent dev servers are permitted to run in the background at production runtime.

# Current Implementation

The scaffold described above is built out under `web-server/`; the repo also holds the
firmware for the field devices that talk to it. Layout of any of them: `ls <dir>/`.

Detailed notes live in per-directory `CLAUDE.md` files, loaded only when working in that
directory:

- `web-server/` - the hub: telemetry, device control, ESP32-CAM streaming, server-owned settings, irrigation scheduler, pump log, AI insight orchestration.
- `esp32cam/` - PlatformIO/Arduino AI-Thinker ESP32-CAM; pushes JPEG frames to the hub.
- `ap-server/` - PlatformIO/Arduino ESP-WROOM-32 SoftAP with a custom DHCP server (MAC->IP reservations).
- `sensor-zone/` - PlatformIO/ESP-IDF ESP-WROOM-32 zone sensor node (DHT22 + soil ADC -> telemetry POST).
- `pump-zone/` - PlatformIO/ESP-IDF ESP-WROOM-32 irrigation-pump relay HTTP server.
- `pump-zone-esp01/` - PlatformIO/Arduino ESP-01/01S drop-in replacement for `pump-zone`.
- `water-temp-node/` - PlatformIO/STM32duino NUCLEO-WL55JC1 battery LoRa water-temp + air/CO2 node.
- `lora-gateway/` - PlatformIO/STM32duino second NUCLEO-WL55JC1, RX-only counterpart, plus the `bridge/` USB->HTTP forwarder.
- `lora-pi-receiver/` - Python SX1278 LoRa gateway on the Raspberry Pi (433 MHz prototype path).
- `smartfarm-ai/` - the AI decision service container (water stress, canopy, disease); see its README. The hub stays AI-agnostic and calls it.

## Commands

- Deploy with real build metadata: `npm run deploy` from `web-server/` (wrapper computes git build number/SHA/date on the host and passes them as compose build args → the Settings **About** panel shows real values; plain `docker compose up --build` builds fine but falls back to `dev`/`unknown`).
