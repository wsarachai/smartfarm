# ap-server — ESP-WROOM-32 SoftAP + custom DHCP with MAC reservations

A PlatformIO/Arduino project that turns an **ESP-WROOM-32** (ESP32-D0WDQ6) into a
Wi-Fi **Access Point** with a **custom DHCP server** and a **web UI** for managing
**MAC→IP reservations**. Devices you register (the "server group") always receive
their assigned address in `192.168.1.2`–`.99`; everyone else gets a dynamic lease
from `192.168.1.100`+. Reservations persist across reboots in NVS.

It grew out of re-implementing the AP behavior of
[`../esp-idf-iot/web-server`](../esp-idf-iot/web-server) (ESP-IDF) on the
**Arduino framework**, to stay consistent with [`../esp32cam`](../esp32cam).

## What it does

1. Brings up a WPA2 SoftAP — **AP-only**, no station/router uplink.
2. **Stops the built-in DHCP server** and runs its **own** (`src/dhcp_server.cpp`)
   on UDP port 67, polled from `loop()`. The built-in ESP DHCP server has **no
   MAC-reservation API**, so a custom one was the only way to pin specific devices
   to specific IPs.
   - **Reserved MAC** → its fixed address in `192.168.1.2`–`.99`.
   - **Unknown MAC** → a dynamic lease from `192.168.1.100`–`.109`.
3. Serves a **web UI** at **http://192.168.1.1/**:
   - a live **dashboard** (auto-refreshing) of connected clients and current
     reservations;
   - an **`/edit` form** to add/update a reservation (label + MAC + IP), with a
     one-click **Reserve** action on any connected client (pre-fills its MAC).
4. **Persists** reservations in NVS via the Arduino `Preferences` library, so they
   survive reboots. Changes take effect on a device's **next DHCP renewal/reconnect**.

## Hardware / board support

- **Board:** generic ESP32 Dev Module → PlatformIO board id **`esp32dev`** on the
  **`espressif32`** platform. Correct target for any ESP-WROOM-32 dev kit. Confirm
  with `pio boards esp32dev`.

## Configuration

All tunables live in [`include/ap_config.h`](include/ap_config.h) — a **committed**
header (no `secrets.h`; a WPA2 PSK is shared with every client anyway). This is a
**standalone AP** distinct from the ESP-IDF reference (`MJU-SmartFarm-AP` @
`192.168.0.1`):

| Setting | Value |
|---|---|
| SSID | `MJU-SmartFarm-AP-II` |
| Password | `password` (WPA2-PSK — **change before real use**) |
| AP IP / gateway | `192.168.1.1` |
| Netmask | `255.255.255.0` |
| Reserved band (static) | `192.168.1.2`–`.99` (MAC reservations) |
| Dynamic DHCP pool | `192.168.1.100`–`.109` (`DHCP_POOL_FIRST_HOST` + max − 1) |
| Lease time | 7200 s (`DHCP_LEASE_SECS`) |
| Max reservations | 32 (`MAX_RESERVATIONS`) |
| Channel | 1 |
| Max simultaneous clients | 10 (`AP_MAX_CONNECTIONS`, ESP32 hardware max) |

## Build, flash, monitor

```bash
cd ap-server
pio run                 # build
pio run -t upload       # flash over USB (port auto-detected)
pio device monitor      # serial log @ 115200
```

If auto-detect grabs the wrong serial port, find yours with `pio device list` and
set `upload_port` / `monitor_port` in `platformio.ini`.

## Try it

1. Flash, open the serial monitor, join Wi-Fi **`MJU-SmartFarm-AP-II`** (password
   `password`), and browse to **http://192.168.1.1/**.
2. Your device shows up under **Connected clients** with a dynamic `.100+` IP.
3. Click **Reserve**, pick an IP in `.2`–`.99` (the form defaults to the next free
   one), optionally name it, and **Save**.
4. Reconnect that device (toggle its Wi-Fi) — it now gets its reserved address.
   Reboot the AP and the reservation is still there.

> **First-boot tip:** because this replaces the DHCP server, during initial
> hardware testing set one laptop to a **static** `192.168.1.50` so you can always
> reach the UI even if DHCP misbehaves.

## Project layout

```
ap-server/
├── platformio.ini          # esp32dev / arduino, single USB env
├── README.md
├── .gitignore
├── include/
│   ├── ap_config.h         # AP + addressing + reservation tunables
│   ├── reservations.h      # NVS-backed MAC→IP store (API)
│   └── dhcp_server.h       # custom DHCP server (API)
└── src/
    ├── main.cpp            # SoftAP bring-up + web UI (dashboard + /edit + CRUD)
    ├── reservations.cpp    # reservation store + MAC parse/format helpers
    └── dhcp_server.cpp     # DISCOVER/REQUEST/RELEASE handling on UDP/67
```

## Notes & scope

- **No auth** on the reservation endpoints — WPA2 is the access control. Anyone on
  the AP can manage reservations. Fine for a private farm AP; add a token if not.
- The custom DHCP server is deliberately minimal (DISCOVER/OFFER, REQUEST/ACK/NAK,
  RELEASE/DECLINE, INFORM), broadcasts replies, and targets a small SoftAP — it is
  **not** a general-purpose DHCP server. **Test on hardware** before relying on it.
- Still no STA uplink, relay control, or OTA (unlike the ESP-IDF reference).
  Natural next steps: STA uplink (`WIFI_AP_STA`), a captive-portal DNS redirect, or
  control endpoints.
