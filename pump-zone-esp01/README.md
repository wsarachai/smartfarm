# pump-zone-esp01

Irrigation-pump relay controller for an **AI-Thinker ESP-01/01S (ESP8266EX)** with a
direct-drive relay. It is a **drop-in replacement for the ESP32 [`../pump-zone`](../pump-zone)**
node: same role in the topology, same `DEVICE_ID` (`pump_zone_01`), same
`/api/v1/relay` HTTP contract — so the Node hub and dashboard need **zero** changes.

## Why this is a rewrite, not a port

`pump-zone` is `framework = espidf`. **ESP-IDF has no ESP8266 target**, so none of
its foundation (`esp_http_server`, `driver/ledc`, cJSON, FreeRTOS-task model,
the pioarduino IDF platform) is available here. This project is rebuilt on the
**Arduino/ESP8266** stack instead:

| Concern        | ESP32 pump-zone (ESP-IDF)        | pump-zone-esp01 (Arduino)         |
| -------------- | -------------------------------- | --------------------------------- |
| HTTP server    | `esp_http_server`                | `ESP8266WebServer`                |
| JSON           | cJSON (bundled)                  | ArduinoJson                       |
| WiFi reconnect | hand-rolled `esp_timer`          | `WiFi.setAutoReconnect(true)`     |
| Status         | RGB LED (LEDC, GPIO25/26/27)     | one onboard LED (GPIO2) blink codes |
| Tasks          | FreeRTOS `main_task` + queue     | classic `setup()` / `loop()`      |

The **HTTP contract is deliberately identical**, so the two are interchangeable in
the field.

## Hardware reality (ESP-01/01S)

The ESP-01 exposes only **GPIO0** and **GPIO2** for use, and both are boot-mode
straps that must idle **HIGH** at power-on. TX/RX (GPIO1/3) carry the serial
console. That entire budget buys exactly **one relay + one LED**:

| Pin   | Use                     | Notes                                                             |
| ----- | ----------------------- | ---------------------------------------------------------------- |
| GPIO0 | Relay (pump), active-HIGH | Also the flash-mode strap — pull LOW to enter download mode.     |
| GPIO2 | Onboard status LED, active-low | Blink codes (see below).                                    |
| TX/RX | Serial log @ 115200     | Free — the direct-drive relay doesn't use UART.                  |

Polarity, pins, HTTP port, and the safety timeout live in the committed
[`include/pump_config.h`](include/pump_config.h). Credentials live in the
gitignored `include/secrets.h` (template: `include/secrets.example.h`).

### Status LED blink codes (GPIO2)

- **Fast blink** — booting / joining WiFi
- **Solid on** — connected, HTTP server up (ready)
- **Slow blip** — WiFi dropped after being up
- **Double-blink heartbeat** — pump running (overrides connectivity)

## HTTP API

Identical to `pump-zone`, plus two ESP-01-specific status fields:

```
POST /api/v1/relay   body: {"state":"on"|"off"}   -> switch the pump
GET  /api/v1/relay                                 -> current state
```

Response: `{"relay_status":"ON"|"OFF"}`, plus while running `"remaining_ms":<n>`
(time until the safety cutoff), and `"safety_off":true` if the last OFF was the
dead-man timer rather than a command. No auth — WPA2 on the AP is the gate.

```bash
curl -X POST http://<ip>/api/v1/relay -d '{"state":"on"}'
curl http://<ip>/api/v1/relay
```

## Safety cutoff (dead-man timer)

The pump auto-shuts **OFF** `PUMP_MAX_RUN_MS` after the last `{"state":"on"}`
(default **5 min**). Each `on` re-arms the countdown, so a long watering run is
just the hub re-POSTing `on` before it expires — a natural heartbeat. A silent or
crashed hub can never leave the pump running longer than one window. Set
`PUMP_MAX_RUN_MS` to `0` to disable (**not** recommended for a real pump).

## Topology / addressing

Joins `MJU-SmartFarm-AP-II` as a STA with a **static IP `192.168.0.5`**
(`USE_STATIC_IP` in `pump_config.h`), so no ap-server MAC reservation is needed
and the address survives a reservation-table wipe. `.5` is inside ap-server's
reserved `.2–.99` range and outside its `.100–.109` dynamic pool, so there's no
lease conflict — just make sure no ap-server reservation also claims `.5`.

The web-server code is left **unchanged** (its default pump target stays
`http://192.168.0.4`). Point it at this node at runtime via the dashboard:
**Settings → Pump Control Settings → Pump URL = `http://192.168.0.5`** and save.
That value persists in the browser's localStorage and overrides the code default —
no rebuild or redeploy of the web-server needed. Set `USE_STATIC_IP` to `0` to
fall back to DHCP.

## Build, flash, run

```bash
cd pump-zone-esp01
cp include/secrets.example.h include/secrets.h    # fill SSID / password / OTA password
pio run                # build
pio run -t upload      # wired flash (see jig note below)
pio device monitor     # 115200
```

### First flash needs a jig ⚠️

The ESP-01 has **no USB and no auto-reset**. To flash over serial you need a
USB-serial (3.3 V!) adapter and must hold **GPIO0 → GND** during power-up to enter
download mode. Because GPIO0 is the relay pin, flash the ESP-01 **off the relay
board** (or on a flashing jig) to avoid disturbing the pump. After the first flash,
use **OTA** for everything else.

### OTA updates (after the first flash)

`ArduinoOTA` is enabled (hostname = `DEVICE_ID`, password = `OTA_PASSWORD` from
`secrets.h`). It force-stops the pump before flashing. To push over WiFi,
uncomment the `espota` block in `platformio.ini`, set `upload_port` to the node's
IP and `--auth=` to your OTA password, then `pio run -t upload`.

## Power note ⚠️

The relay coil plus the ESP8266's WiFi current bursts can brown out a weak 3.3 V
supply, causing resets when the relay pulls in. Use a supply/regulator with
headroom (a common failure mode on ESP-01 relay boards).

## Status

**Not yet compiled or hardware-verified.** Structure, pin map, and contract are
in place; run `pio run` on the target toolchain and bench-test the relay + safety
cutoff before deploying to a real pump.
