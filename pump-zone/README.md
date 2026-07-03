# pump-zone — ESP-WROOM-32 irrigation pump control node

A PlatformIO/**ESP-IDF** project that turns an **ESP-WROOM-32** (ESP32-D0WDQ6)
into a network-controlled irrigation pump switch. It joins WiFi as a **STA**,
runs a small **HTTP server** to toggle a **relay** (the pump), and reflects
state on an **RGB status LED**.

It is a stripped, modernized PlatformIO/ESP-IDF port of the reference
[`../esp-idf-iot/web-server`](../esp-idf-iot/web-server), keeping **only** the
relay switching + LED status; the SoftAP, OTA, and other machinery are dropped.
It builds and flashes with the same `pio` tooling as its sibling projects
[`../sensor-zone`](../sensor-zone), [`../ap-server`](../ap-server), and
[`../esp32cam`](../esp32cam).

## HTTP API

The node exposes a tiny JSON control API on port 80 (configurable via
`PUMP_HTTP_PORT` in [`include/pump_config.h`](include/pump_config.h)).

| Method | Path              | Body                    | Response                    |
|--------|-------------------|-------------------------|-----------------------------|
| `POST` | `/api/v1/relay`   | `{"state":"on"}` / `{"state":"off"}` | `{"relay_status":"ON"}` / `{"relay_status":"OFF"}` |
| `GET`  | `/api/v1/relay`   | —                       | `{"relay_status":"ON"}` / `{"relay_status":"OFF"}` (current state) |

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
```

## Wiring

| Function        | GPIO   | Notes                                             |
|-----------------|--------|---------------------------------------------------|
| Relay (pump)    | GPIO23 | Active-**low** by default (`RELAY_ACTIVE_LEVEL 0`); moved off strapping pin GPIO2 |
| RGB LED — Red   | GPIO25 | LEDC PWM, active-high                              |
| RGB LED — Green | GPIO26 | LEDC PWM, active-high                              |
| RGB LED — Blue  | GPIO27 | LEDC PWM, active-high                              |

All pins and relay polarity are set in the committed, non-secret header
[`include/pump_config.h`](include/pump_config.h). Most cheap relay boards are
active-low (GPIO low = relay energized); flip `RELAY_ACTIVE_LEVEL` to `1` for an
active-high board.

## LED status

| Color   | Meaning                        |
|---------|--------------------------------|
| BLUE    | Booting                        |
| GREEN   | WiFi connected + HTTP server up|
| RED     | WiFi disconnected              |
| MAGENTA | Pump relay ON                  |

## Network

- **STA-only.** The node joins the AP defined in `include/secrets.h`
  (default `MJU-SmartFarm-AP-II`) and receives its IP via **DHCP**.
- For a stable address, **reserve its MAC** in
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
the template.

## Commands

```
pio run                 # build the firmware (env:esp32dev)
pio run -t upload       # build + flash over USB
pio device monitor      # serial monitor @ 115200 baud
```

The first `pio run` pulls the pioarduino platform + IDF 5.3.2 (slow, one-time).

## Status

**Not yet compiled or hardware-verified.** The PlatformIO/ESP-IDF configuration
here has not been built or flashed on real hardware in this environment; run
`pio run` and flash on an actual ESP-WROOM-32 before relying on it.
