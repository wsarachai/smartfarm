# pump-zone — ESP-WROOM-32 irrigation pump control node

A PlatformIO/**ESP-IDF** project that turns an **ESP-WROOM-32**
(ESP32-D0WDQ6) into a network-controlled irrigation pump node. It joins WiFi
as a **STA**, starts an **HTTP server** after it receives an IP address,
switches a **relay** for the pump, and reflects runtime state on an **RGB
status LED**.

This is a stripped PlatformIO/ESP-IDF port of the reference
[`../esp-idf-iot/web-server`](../esp-idf-iot/web-server), keeping only the
relay control path and LED status indication. SoftAP, OTA, and other extra
machinery are intentionally omitted. It builds and flashes with the same `pio`
tooling as its sibling projects [`../sensor-zone`](../sensor-zone),
[`../ap-server`](../ap-server), and [`../esp32cam`](../esp32cam).

## Runtime behavior

- Boot state is shown in **BLUE**.
- When WiFi disconnects, the node shows **RED** and retries automatically.
- When WiFi is connected and the HTTP server is up, the node shows **GREEN**.
- When the pump relay is on, the LED is temporarily overridden to **MAGENTA**.
- The relay defaults to **OFF** on boot.
- WiFi reconnects automatically after a 1 second delay.
- The HTTP server starts only after the station gets an IP address and is
  started idempotently on reconnect.

## HTTP API

The node exposes a tiny JSON control API on port 80, configurable via
`PUMP_HTTP_PORT` in [`include/pump_config.h`](include/pump_config.h).

| Method | Path              | Body                    | Response                    |
|--------|-------------------|-------------------------|-----------------------------|
| `GET`  | `/`               | —                       | `{"device_id":"pump_zone_01","health":{"status":"ok|degraded","wifi_connected":true|false,"http_server":true|false,"relay":"ON|OFF","ip":"x.x.x.x|unavailable","uptime_ms":12345}}` |
| `POST` | `/api/v1/relay`   | `{"state":"on"}` / `{"state":"off"}` | `{"relay_status":"ON"}` / `{"relay_status":"OFF"}` |
| `GET`  | `/api/v1/relay`   | —                       | `{"relay_status":"ON"}` / `{"relay_status":"OFF"}` (current state) |

Behavior details:

- Requests must send a small JSON body; oversized payloads are rejected.
- Invalid JSON, missing `state`, or unknown values return `400`.
- A relay control failure returns `500`.
- The response is always the current relay state as JSON.
- `GET /` returns device health with connectivity, relay state, IP, and uptime.

Examples (assuming the node landed at `192.168.1.20`):

```
# Turn the pump ON
curl -X POST http://192.168.1.20/api/v1/relay \
     -H 'Content-Type: application/json' \
     -d '{"state":"on"}'
# => {"relay_status":"ON"}

# Turn the pump OFF
curl -X POST http://192.168.1.20/api/v1/relay \
     -H 'Content-Type: application/json' \
     -d '{"state":"off"}'
# => {"relay_status":"OFF"}

# Query current state
curl http://192.168.1.20/api/v1/relay
# => {"relay_status":"OFF"}

# Query device health
curl http://192.168.1.20/
# => {"device_id":"pump_zone_01","health":{"status":"ok","wifi_connected":true,"http_server":true,"relay":"OFF","ip":"192.168.1.20","uptime_ms":12345}}
```

## Wiring

| Function        | GPIO   | Notes |
|-----------------|--------|-------|
| Relay (pump)    | GPIO23 | Active-**low** by default (`RELAY_ACTIVE_LEVEL 0`); moved off strapping pin GPIO2 |
| RGB LED — Red   | GPIO25 | LEDC PWM, active-high |
| RGB LED — Green | GPIO26 | LEDC PWM, active-high |
| RGB LED — Blue  | GPIO27 | LEDC PWM, active-high |

All pins and relay polarity are defined in
[`include/pump_config.h`](include/pump_config.h). Most cheap relay boards are
active-low, so GPIO low energizes the relay. Flip `RELAY_ACTIVE_LEVEL` to `1`
for an active-high board.

## LED status

| Color   | Meaning |
|---------|---------|
| BLUE    | Booting / connecting |
| GREEN   | WiFi connected and HTTP server up |
| RED     | WiFi disconnected |
| MAGENTA | Pump relay on |

## Network

- **STA-only.** The node joins the AP defined in `include/secrets.h`
  (default `MJU-SmartFarm-AP-II`) and receives its IP via **DHCP**.
- If the AP drops, the firmware logs the disconnect reason, sets the LED to
  red, and retries automatically.
- For a stable address, reserve the device MAC in
  [`../ap-server`](../ap-server)'s web UI so it lands in the `.2`–`.99` server
  group rather than the dynamic pool.

## Setup

Credentials live in a gitignored header. Copy the template and fill in your
Wi-Fi settings and device id before the first build:

```
cp include/secrets.example.h include/secrets.h
# then edit include/secrets.h
```

`include/secrets.h` is git-ignored; `include/secrets.example.h` is tracked as
the template. The template defaults to `MJU-SmartFarm-AP-II`, `password`, and
`pump_zone_01`.

## Commands

```
pio run                 # build the firmware (env:esp32dev)
pio run -t upload       # build + flash over USB
pio device monitor      # serial monitor @ 115200 baud
```

The first `pio run` pulls the pioarduino platform plus IDF 5.3.2, so expect a
slow one-time download.

## Implementation Notes

- `src/main.c` owns the main task, queue, and LED state transitions.
- `src/network/wifi_sta.c` handles WiFi init, disconnect handling, and automatic
  reconnects.
- `src/http/http_server.c` starts the server once WiFi is up and registers the
  relay routes.
- `src/http/http_server_relay.c` validates the JSON body and maps `/api/v1/relay`
  to relay state changes.
- `src/actuators/relay.c` drives the pump relay on GPIO23.
- `src/actuators/rgb_led.c` drives the RGB status LED with LEDC PWM.
