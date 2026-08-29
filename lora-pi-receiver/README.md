# lora-pi-receiver

Raspberry Pi **LoRa gateway** for the Smart Farm: an SX1278 wired to the Pi's SPI0
receives the sensor node's LoRa frames and POSTs telemetry to the web-server.
Replaces the separate WL55 gateway + USB bridge for the **433 MHz prototype path**.

```
water-temp-node (F103 + SX1278 @433, DS18B20)
        │  LoRa: 12/18/20/28/36-byte frame
        ▼
Raspberry Pi 3 ── SX1278 on SPI0 ── receiver.py ──HTTP──> web-server:3000
                                     decode → {device_id, metrics} → /api/v1/telemetry
```

Standalone by design (like `lora-gateway/bridge` and the AI poller) — the
**web-server needs zero changes**. HTTP uses the stdlib; the only pip deps are
`spidev` + `gpiozero` (+ `lgpio` on Bookworm).

> **433 MHz prototype.** SX1278 is a 433 MHz part — this path does **not**
> interoperate with the 923 MHz WL55 gateway (that remains the separate AS923
> option). The whole point here is a full sensor→dashboard path on the hardware
> you have while the WL55 boards are on back-order.

## Wiring — SX1278 → Raspberry Pi 3 (SPI0)

3.3 V **only** (never 5 V), and **attach an antenna** before the node transmits
near it. BCM numbering / physical header pin:

| SX1278 | Pi (BCM)    | Pin | Note        |
|--------|-------------|-----|-------------|
| VCC    | 3V3         | 1   | 3.3 V only  |
| GND    | GND         | 6   |             |
| NSS    | GPIO8 / CE0 | 24  | `/dev/spidev0.0` auto-manages CS |
| SCK    | GPIO11      | 23  | SPI0 SCLK   |
| MISO   | GPIO9       | 21  |             |
| MOSI   | GPIO10      | 19  |             |
| RESET  | GPIO22      | 15  | `RESET_PIN` |
| DIO0   | GPIO25      | 22  | `DIO0_PIN`  |
| ANT    | antenna     | —   | required    |

## Setup (on the Pi)

```bash
# 1. Enable SPI
sudo raspi-config          # Interface Options -> SPI -> Enable   (or: dtparam=spi=on)

# 2. Install the deps.
#    Raspberry Pi OS Bookworm blocks system-wide `pip` (PEP 668 /
#    "externally-managed-environment"). Our 3 deps are all Debian-packaged and are
#    hardware libs, so apt is the simplest + most reliable path (no venv needed):
sudo apt update
sudo apt install -y python3-spidev python3-gpiozero python3-lgpio

#    ...OR, if you prefer an isolated venv (use --system-site-packages so it still
#    sees the apt hardware backends):
#      python3 -m venv --system-site-packages .venv
#      .venv/bin/pip install -r requirements.txt
#      # then run with .venv/bin/python and point the systemd ExecStart there.

# 3. Config + run (foreground test first)
cd /opt/smartfarm/lora-pi-receiver   # (or wherever you cloned it)
cp .env.example .env                 # edit if needed (PHY must match the node)
python3 receiver.py
```

Expected output — the first line is your **wiring check**:

```
2026-... SX127x RegVersion=0x12 (expect 0x12) -> OK
2026-... listening @ 433.000 MHz SF9 BW-code 7 CR-code 1 sync 0x12 -> http://localhost:3000/api/v1/telemetry
2026-... # node 1: frame v5, 6/6 probes and 3/3 air sensors reading, air temp/humidity from SHT45
2026-... -> water-temp-01 {"temp_hot": 23.75, "temp_cold": 23.87, "temp_p2": 23.1, "temp_p3": 23.44, "air_temp": 24.13, "humidity": 58.2, "pressure": 1013.2, "co2": 812, "rssi": -41, "snr": 9.2, "seq": 5}
```

`RegVersion=0x12` confirms SPI + wiring. `0x00`/`0xFF` means SPI isn't enabled or
the SPI wires are wrong.

## Run as a service (auto-start, restart on failure)

```bash
sudo cp lora-pi-receiver.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now lora-pi-receiver
journalctl -u lora-pi-receiver -f          # live logs
```

The unit reads `.env` via `EnvironmentFile` and POSTs to the Dockerized
web-server on `localhost:3000`. The service user needs access to `/dev/spidev0.0`
and the gpiochip (add it to the `spi`/`gpio` groups, or run as root).

## Config (`.env`)

| Var | Default | Note |
|-----|---------|------|
| `SERVER_URL` | `http://localhost:3000/api/v1/telemetry` | web-server endpoint |
| `LORA_FREQ_HZ` | `433000000` | **must match the node** |
| `LORA_SF` / `LORA_BW_CODE` / `LORA_CR_CODE` | `9` / `7` / `1` | SF9 / 125 kHz / 4-5 |
| `LORA_PREAMBLE` / `LORA_SYNCWORD` | `8` / `0x12` | must match the node |
| `SPI_BUS` / `SPI_DEV` | `0` / `0` | `/dev/spidev0.0` (CE0) |
| `RESET_PIN` / `DIO0_PIN` | `22` / `25` | BCM |
| `NODE_MAP` | `{"1":"water-temp-01"}` | node_id → dashboard id |
| `PROBE_METRICS` | `["temp_hot","temp_cold","temp_p2",…]` | probe index → metric name; JSON list of exactly 6 |
| `AIR_METRICS` | `["air_temp","air_temp_2","air_temp_3"]` | air sensor index → metric name; JSON list of exactly 3 |
| `HUM_METRICS` | `["humidity","humidity_2","humidity_3"]` | air sensor index → humidity metric name; exactly 3 |
| `ALTITUDE_M` | `0` | site elevation (m); >0 adds `pressure_msl` (sea-level-reduced) |

## Frame versions
- **v1** (magic `0xA1`, 12 B): DS18B20 water temps + battery only.
- **v2** (magic `0xA2`, 18 B): adds an ambient block — `air_temp` (°C), `humidity` (%), `pressure` (hPa), from a **BME280** (I²C2, addr 0x76).
- **v3** (magic `0xA3`, 20 B): adds `co2` (ppm) from an **SCD41** (addr 0x62), and takes `air_temp`/`humidity` from an **SHT45** (addr 0x44) instead of the BME280, which then supplies pressure only.
- **v4** (magic `0xA4`, 28 B): widens the water temperatures from **2 probes to 6**, appending probes 2–5 after the CO2 field.
- **v5** (magic `0xA5`, 36 B): widens the air sensors from **1 SHT45 to 3**, appending sensors 1 and 2 (temp + humidity each) after the probes. On the node they share one factory-fixed I2C address and are separated by a bus switch, but that is invisible on the wire.

`lora_packet.py` decodes **all five** — no receiver change is needed to keep older nodes working. Each version only appends, so a field never moves and `receiver.py` emits a metric only when the frame says that field is valid.

**Probe naming** is `PROBE_METRICS` in `receiver.py` (overridable from the env as a JSON list). Probes 0 and 1 default to `temp_hot` / `temp_cold` because they occupy the same wire slots they always did, so an existing dashboard's history stays one continuous series; probes 2–5 arrive as `temp_p2`…`temp_p5`. Rename them per install — but a renamed probe starts a **new** series. Keep this list in step with `gw_probe_metric()` in `lora-gateway/include/gateway_config.h` if both gateways feed the same dashboard.

**v4 and v5 add no flag bits, and cannot** — all eight are already used. The added channels signal "no reading" with sentinels: `0x8000` for temperatures (as probes 0/1 already did) and `0xFFFF` for humidity, a value outside its 0..10000 range. So `lora_packet.probe_valid(pkt, i)`, `air_valid(pkt, i)` and `hum_valid(pkt, i)` are the single places that know which slot uses which mechanism. Call them rather than testing flags or sentinels by hand.

**`FLAG_SHT` (bit 7)** marks air temp/humidity as SHT45-sourced. The metric names and units are identical either way, so it is deliberately **not** a metric — dashboard history stays continuous across a sensor swap. `receiver.py` logs it once per node, and again only if it changes, so a dead SHT45 falling back to the BME280 is visible in the journal without adding a line per frame.

### Testing the decoder
`test_lora_packet.py` checks byte-compatibility against vectors generated by compiling the firmware's own `lora_packet.h` on the host, covering v1 through v5, negative temperatures, a partly-populated six-probe node, a three-SHT45 node with only one sensor answering, a slot-0 failure, the `probe_valid()` / `air_valid()` / `hum_valid()` rules, and every single-byte corruption. It is stdlib-only — no Pi and no radio needed:

```bash
cd lora-pi-receiver && python3 -m unittest test_lora_packet -v
```

Run it after any change to the wire format, on either side.

## Files
- `sx127x.py` — SX1278 driver (spidev + gpiozero); Python port of the node's `sx1278.cpp`.
- `lora_packet.py` — decode of the v1–v5 frame; port of `lora_packet.h` (kept byte-compatible).
- `test_lora_packet.py` — decoder unit tests against firmware-generated vectors.
- `receiver.py` — RX loop → decode → map → POST.
- `lora-pi-receiver.service` — systemd unit. `requirements.txt`, `.env.example`.

## Status
Firmware side (F103 node sender) compiles clean. **The Pi receiver is not yet
run on hardware** (spidev/gpiozero only work on a real Pi) — the driver mirrors
the register sequence already validated on the node's SX1278. Do the first
`python3 receiver.py` on the Pi and confirm `RegVersion=0x12`.
