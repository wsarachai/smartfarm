# ap-server — ESP-WROOM-32 SoftAP + DHCP + live status page

A minimal PlatformIO/Arduino project that turns an **ESP-WROOM-32** (ESP32-D0WDQ6)
into a Wi-Fi **Access Point with a DHCP server** and a tiny web page that shows,
live, which clients have joined and what IP each was leased.

It re-implements the AP + DHCP behavior of
[`../esp-idf-iot/web-server`](../esp-idf-iot/web-server) (which is ESP-IDF) on the
**Arduino framework**, to stay consistent with the other PlatformIO project in
this repo, [`../esp32cam`](../esp32cam).

## What it does

1. Brings up a WPA2 SoftAP — **AP-only**, no station/router uplink.
2. `WiFi.softAPConfig()` pins the AP IP to `192.168.1.1/24`. The Arduino-ESP32
   core then **auto-starts a DHCP server** on the AP interface and hands out
   `192.168.1.2`, `.3`, … to clients as they associate.
3. A synchronous `WebServer` serves one auto-refreshing status page at
   **http://192.168.1.1/** listing each connected station's **MAC** and
   **leased IP** (joined from the Wi-Fi driver's station list and the netif DHCP
   lease table).

Any path (not just `/`) returns the status page.

## Hardware / board support

- **Board:** generic ESP32 Dev Module → PlatformIO board id **`esp32dev`** on the
  **`espressif32`** platform. This is the correct target for any ESP-WROOM-32
  dev kit. Confirm locally with `pio boards esp32dev`.

## Configuration

All tunables live in [`include/ap_config.h`](include/ap_config.h) — a **committed**
header. Nothing here is truly secret: a WPA2 pre-shared key is shared with every
client that joins, so there is no `secrets.h`. This is a **standalone AP on its
own network** — distinct from the ESP-IDF reference (`MJU-SmartFarm-AP` @
`192.168.0.1`); clients must join *this* SSID to reach it:

| Setting | Value |
|---|---|
| SSID | `MJU-SmartFarm-AP-II` |
| Password | `password` (WPA2-PSK — **change before real use**) |
| AP IP / gateway | `192.168.1.1` |
| Netmask | `255.255.255.0` |
| DHCP pool | auto, `192.168.1.2`+ |
| Channel | 1 |
| Max clients | 5 |

## Build, flash, monitor

```bash
cd ap-server
pio run                 # build
pio run -t upload       # flash over USB (port auto-detected)
pio device monitor      # serial log @ 115200
```

If auto-detect grabs the wrong serial port, find yours with `pio device list`
and uncomment/set `upload_port` / `monitor_port` in `platformio.ini`.

## Try it

1. Flash and open the serial monitor — you'll see the AP come up and the status
   URL printed.
2. On a phone/laptop, join Wi-Fi **`MJU-SmartFarm-AP-II`** (password `password`).
3. Browse to **http://192.168.1.1/** — your device appears in the table with the
   IP the DHCP server leased it. The page refreshes every few seconds, so more
   clients pop in live as they join.

## Project layout

```
ap-server/
├── platformio.ini      # esp32dev / arduino, single USB env
├── README.md
├── .gitignore
├── include/
│   └── ap_config.h     # AP SSID/pass/IP/channel/max clients
└── src/
    └── main.cpp        # softAP + softAPConfig + WebServer status page
```

## Scope / next steps

This first version is deliberately just **AP + DHCP + status page**. It does *not*
port the reference web-server's HTTP control UI, STA credential configuration,
relay control, OTA, or NVS. Natural extensions from here: add an STA uplink
(`WIFI_AP_STA`), a captive-portal DNS redirect, or REST endpoints for control.
