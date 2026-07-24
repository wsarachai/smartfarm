# ESP32-CAM IP Camera (v1)

A LAN-only IP camera on the AI-Thinker **ESP32-CAM**, flashed via the
**ESP32-CAM-MB** (CH340G) adapter. Live MJPEG stream + on-demand snapshot,
viewable in any browser.

## Camera modules

The same AI-Thinker board ships with different sensors behind the ribbon. Two
are supported, each with its own build env — **flash the one that matches your
module**:

| Module | Sensor | Env | JPEG | Notes |
|---|---|---|---|---|
| Original | OV3660 / OV2640 | `esp32cam` | **Hardware** | UXGA, the v1 tuning, unchanged |
| RHYX M21-45 | GC2145 | `esp32cam_rhyx` | **Software** | SVGA max, no image sliders |

The socket and pin map are identical, so this is a **build-flag choice**
(`-DCAMERA_MODULE_*` in `platformio.ini`, profiles in `src/camera_module.h`),
not rewiring.

**Why the RHYX needs its own build:** the GC2145 has **no hardware JPEG
encoder** — it only emits RGB565/YUV422. Asking `esp_camera_init()` for
`PIXFORMAT_JPEG` (what the OV profile does) fails with
`ESP_ERR_NOT_SUPPORTED`, which would boot-loop. The `esp32cam_rhyx` profile
instead captures RGB565 and compresses to JPEG **on the CPU** (`frame2jpg`),
so everything downstream — the stream, `/capture`, the snapshot push to the hub —
still sees a normal JPEG. Consequences of software encoding:

- **Resolution ceiling is SVGA** (800×600). RGB565 is 2 bytes/px, so UXGA would
  need a 3.84 MB frame buffer (out of 4 MB PSRAM) and overflow the encoder's
  fixed 128 KB output buffer. The ceiling is enforced at runtime, and the web
  UI hides the resolutions this module can't reach.
- **No brightness / contrast / saturation** — the GC2145 driver doesn't
  implement them (returns −1). Those sliders are greyed out in the UI.
- **Quality** still uses the familiar hardware scale (4–63, lower = better);
  the firmware maps it onto the software encoder internally, so the hub's
  `jpeg_quality` config and the UI slider work unchanged.

If you flash the wrong env (OV profile on an RHYX board), the firmware
**detects the rejected-JPEG failure at boot and falls back to the software
path automatically** so the camera still comes up — but flash `esp32cam_rhyx`
for the correct defaults and ceiling. The serial log and the web UI header both
name the module actually detected.

## Design (v1)

| Decision | Choice |
|---|---|
| Toolchain | PlatformIO + Arduino (`board = esp32cam`) |
| Consumption | Browser MJPEG stream + web control UI |
| Base code | Stock `CameraWebServer`, **face detection stripped out** |
| Scope | Live viewer + snapshot + dimmable flash LED (no SD, no motion) |
| Camera default | UXGA 1600×1200, JPEG quality 12, 10 MHz XCLK, `GRAB_WHEN_EMPTY` (tuned for clean AI frames over FPS; needs solid 5V power) |
| WiFi | Hardcoded in git-ignored `include/secrets.h` |
| Addressing | Static IP in firmware + mDNS (`esp32cam.local`) |
| Partition | `min_spiffs.csv` (dual ~1.9 MB app slots, OTA-capable) |
| Updates | USB (first flash) then wireless OTA (password-protected) |
| Security | LAN-only, no auth — **never port-forward this** |
| Power | Dev via MB micro-USB; deploy on a dedicated **5V 2A** supply |

## First-time setup

1. Copy the secrets template and fill in your network:
   ```
   cp include/secrets.example.h include/secrets.h
   ```
   Edit `include/secrets.h` — set `WIFI_SSID`, `WIFI_PASS`, and an
   `OTA_PASSWORD`. The camera gets its IP via **DHCP** (assigned by ap-server);
   to pin a fixed address, add a MAC reservation in ap-server instead. The MAC
   is printed on the serial monitor at boot.

2. First flash over USB (ESP32-CAM-MB plugged in). Pick the env for your
   module (see the table above):
   ```
   pio run -e esp32cam --target upload        # OV3660 / OV2640
   pio run -e esp32cam_rhyx --target upload    # RHYX M21-45 (GC2145)
   pio device monitor
   ```
   The MB adapter handles boot mode + auto-reset — no GPIO0 jumper needed.

3. Watch the serial monitor (115200) for the sensor type and the line:
   ```
   [http] ready:  http://172.16.1.11/    stream on :81
   ```
   Open that URL (or `http://esp32cam.local/`) in a browser.

## Updating over the air (no cable)

Once the OTA-capable firmware is on the board (step 2 above), push future
updates wirelessly from any machine on the same WiFi:

```
pio run -e esp32cam_ota --target upload        # OV3660 / OV2640
pio run -e esp32cam_rhyx_ota --target upload    # RHYX M21-45 (GC2145)
```

- Target IP and `--auth` password live in `platformio.ini` under `[env:esp32cam_ota]`
  (the RHYX OTA env extends `esp32cam_rhyx` and reuses the same IP/auth).
- The `--auth` value **must match** `OTA_PASSWORD` in `include/secrets.h`.
- Changing the OTA password requires one USB reflash (the board authenticates
  the incoming update against the password it's *currently* running).
- A partition-table change (e.g. going back to non-OTA) also needs USB.

## Endpoints

| URL | Purpose |
|---|---|
| `http://<ip>/` | Control UI (port 80) |
| `http://<ip>:81/stream` | MJPEG stream |
| `http://<ip>/capture` | Single JPEG snapshot |
| `http://<ip>/control?var=<name>&val=<n>` | Live sensor tweak (incl. `led_intensity` 0–255) |
| `http://<ip>/status` | Current sensor settings (JSON) |

## Troubleshooting

- **Reboots / brownouts / garbage frames** → power first. Use a real 5V 2A
  supply and a short, thick cable. Weak USB ports sag under the camera's
  current spikes.
- **`camera init failed`** → reseat the ribbon cable; confirm power.
- **`sensor rejected JPEG — no hardware encoder fitted`** in the boot log → the
  board has an **RHYX M21-45 (GC2145)** but was flashed with the OV env. It
  auto-falls back to software JPEG so it still works; reflash `esp32cam_rhyx`
  for the correct SVGA ceiling and defaults.
- **Resolution won't go above SVGA / image sliders do nothing** → expected on
  the RHYX M21-45; the GC2145 encodes in software (SVGA max) and has no
  brightness/contrast/saturation controls. See *Camera modules* above.
- **Can't reach `esp32cam.local`** → mDNS is flaky on some Android/Windows
  setups; use the DHCP-assigned IP directly (printed on the serial monitor at
  boot, or visible in ap-server's client list).
- **Stream stutters** → drop resolution to VGA in the UI, or move closer to
  the AP (check the RSSI printed at boot).

## Roadmap (deliberately out of v1)

- SD-card snapshot/recording (note: SD shares GPIO4 with the flash LED, so
  the two can't be used together without rework)
- HTTP Basic Auth / Tailscale for safe remote access
