# sensor-zone — ESP-WROOM-32 farm-zone sensor node

A PlatformIO/**ESP-IDF** project that turns an **ESP-WROOM-32** (ESP32-D0WDQ6)
into a field sensor node for one farm zone. It is a port of the legacy
[`../esp-idf-iot/sensor-node`](../esp-idf-iot/sensor-node) (a raw ESP-IDF
workspace) onto PlatformIO, so it builds and flashes with the same `pio`
tooling as its sibling projects [`../esp32cam`](../esp32cam) and
[`../ap-server`](../ap-server).

The node reads its sensors and reports telemetry up to the Smart Farm
web-server (`POST /api/v1/telemetry`).

## Hardware / sensors

| Function        | Signal / channel      | GPIO   |
|-----------------|-----------------------|--------|
| DHT22 (temp/RH) | 1-wire digital        | GPIO32 |
| Soil moisture   | ADC1_CH6 (analog)     | GPIO34 |
| RGB LED — Red   | digital / PWM         | GPIO25 |
| RGB LED — Green | digital / PWM         | GPIO26 |
| RGB LED — Blue  | digital / PWM         | GPIO27 |
| Status LED      | digital (onboard)     | GPIO2  |

GPIO34 is input-only (ADC1_CH6), which is correct for the analog soil probe.

## Board / platform

- **Board:** generic ESP32 Dev Module → PlatformIO board id **`esp32dev`**.
- **Platform:** the **pioarduino** fork of `platform-espressif32`, pinned to tag
  **`53.03.13`** (Arduino core 3.1.3 based on **ESP-IDF 5.3.2**). We use this
  fork rather than the stock `espressif32` platform because the ported source
  needs **IDF >= 5.2** for the new ADC oneshot constant `ADC_ATTEN_DB_12`; the
  stock platform still ships an older IDF.
- **Framework:** `espidf`. The source is a single `main` component flattened
  into `src/` (with its own `src/CMakeLists.txt`); PlatformIO auto-generates the
  top-level CMake, so there is no root `CMakeLists.txt`.

Build settings are pinned in [`sdkconfig.defaults`](sdkconfig.defaults)
(target `esp32`, single-app partition, 4MB flash, 160 MHz, 100 Hz tick, INFO
logs) for reproducible builds — matching the legacy node except flash bumped
2MB → 4MB to match real devkits.

## Setup

Credentials live in a gitignored header. Copy the template and fill in your
Wi-Fi (and OTA) settings before the first build:

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
pio test -e native      # host-side Unity unit tests (pure logic)
```

## Status

**Build not yet hardware-verified.** The PlatformIO/ESP-IDF configuration here
has not been compiled or flashed on real hardware in this environment; verify
`pio run` and a flash on an actual ESP-WROOM-32 before relying on it.
