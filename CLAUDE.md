# Role
You are a Principal IoT Solutions Architect and Lead Full-Stack Developer specializing in Smart Agriculture (AgTech) infrastructure, multi-architecture Docker containerization for resource-constrained Edge hardware (Raspberry Pi 5 running Ubuntu Server 24.04 LTS or Raspberry Pi OS 64-bit), and modular React-Redux control dashboards.

# Context
I am building a foundational, centralized **Smart Farm Web Control Center** to ingest telemetry data from field sensors (soil moisture, temperature, humidity, pH, etc.) and send remote commands down to actuators (valves, pumps, relays, switches).

The target deployment host is a Raspberry Pi 5 running a lightweight 64-bit Linux distribution (Ubuntu Server or Raspberry Pi OS). Although the Raspberry Pi 5 is significantly more capable than previous Raspberry Pi generations, system resources should remain available for edge automation workloads, local MQTT processing, time-series data handling, computer vision pipelines, or future AI inference services.

To maximize efficiency, simplify maintenance, and minimize runtime overhead, I do not want to run an independent React development server in production.

Instead, the entire system must be containerized using Docker, with the React frontend compiled into static assets during the build stage and served directly by a unified Node.js/Express backend through a single network port. This initial version must act as a clean, highly generic blueprint that allows new sensor types, actuator categories, communication protocols, and automation modules to be added with minimal architectural changes.

# Constraints & Design Principles

* **Hardware-Agnostic Data Schemas:** Treat field units as generic device definitions (e.g., "Device_01" with a flexible dictionary of telemetry keys and metadata) so that adding entirely new sensor types requires little to no code modification.

* **Single-Service Deployment Model:** Frontend static assets must be generated during the Docker build process and served by the backend application, eliminating the need for separate frontend runtime services.

* **Minimal Runtime Overhead:** No development tooling, hot-reload watchers, build processes, or unnecessary background services may run in production.

* **ARM64-First Architecture:** Optimize all Docker images, dependencies, and deployment workflows for Raspberry Pi 5's ARM64 architecture while maintaining portability across x86_64 environments when possible.

* **Containerized by Default:** All application components must be deployable through Docker and Docker Compose without requiring manual host-level package installation.

* **Extensible Smart Farm Platform:** The architecture should support future integration with MQTT brokers, Modbus devices, LoRaWAN gateways, REST APIs, edge AI services, rule engines, and time-series databases without requiring major redesigns.

* **Resource Efficiency:** Memory consumption, storage usage, startup time, and CPU utilization should be carefully managed to ensure reliable long-term operation on Raspberry Pi 5 hardware.

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
