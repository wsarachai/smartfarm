# lora-gateway

Receive-only **LoRa gateway** on a second **NUCLEO-WL55JC1**, plus a small Node.js
**bridge** that forwards to the Smart Farm web-server. Together they close the gap
between the LoRa field node and the HTTP-only web-server.

```
water-temp-node --LoRa AS923--> lora-gateway --USB CDC JSON--> bridge.js --HTTP--> web-server:3000
```

The gateway listens continuously on the shared channel, validates each frame's
CRC-8, maps `node_id → device_id`, and prints **one JSON line per frame** out
**LPUART1** (the ST-LINK virtual COM port). The bridge reads those lines and
POSTs them to `/api/v1/telemetry`.

## Firmware (the WL55 gateway)

Same board/toolchain as [`../water-temp-node`](../water-temp-node) — PlatformIO
`framework = arduino` (STM32duino, which *is* the STM32Cube HAL underneath; see
the node README for why not `stm32cube`). It shares the LoRa code byte-for-byte —
`src/lora/{lora_params.h,lora_packet.h,subghz_lora.*}` and
`include/hal_conf_extra.h` are **copies** kept in sync with the node. If you
change the PHY or packet format, change **both** copies.

Output line format (rssi/snr added here from the LoRa RX; invalid temps omitted):

```
{"device_id":"water-temp-01","metrics":{"temp_hot":41.30,"temp_cold":22.60,"temp_p2":23.10,"temp_p3":23.44,"battery_v":3.140,"air_temp":24.13,"humidity":58.20,"air_temp_2":25.01,"humidity_2":64.10,"co2":812,"rssi":-92,"snr":8.5,"seq":42}}
```

Which metrics appear depends on the frame version and its valid-flags — the node
decides, the gateway only forwards:

| Version | Magic | Bytes | Metrics it can carry |
|---|---|---|---|
| v1 | `0xA1` | 12 | `temp_hot`, `temp_cold`, `battery_v` |
| v2 | `0xA2` | 18 | + `air_temp`, `humidity`, `pressure` |
| v3 | `0xA3` | 20 | + `co2` |
| v4 | `0xA4` | 28 | + `temp_p2`…`temp_p5` (2 probes → **6**) |
| v5 | `0xA5` | 36 | + `air_temp_2/3`, `humidity_2/3` (1 SHT45 → **3**) |

Each version only **appends**, so a field never moves and `lora_packet_unpack()`
accepts all five. An older node in the field keeps working against a current
gateway with no change at either end, and keeps its metric names.

**Metric naming lives in [`include/gateway_config.h`](include/gateway_config.h)**
(`gw_probe_metric()`, `gw_air_metric()`, `gw_hum_metric()`), not in the JSON
builder. Probe 0/1 and air sensor 0 default to their historic names — `temp_hot`,
`temp_cold`, `air_temp`, `humidity` — because they occupy the same wire slots they
always did, so the dashboard's existing history stays one continuous series.
Rename them for your install — but a renamed channel starts a **new** series.

A probe or air sensor that is absent, unfitted or failed is simply **not
emitted**: v4 and v5 have no spare flag bits, so the added channels carry
validity as sentinels (`0x8000` for temperatures, `0xFFFF` for humidity), and
`lora_probe_valid()` / `lora_air_valid()` / `lora_hum_valid()` are the only
places that know which slot uses which mechanism.

Lines starting with `#` are diagnostics (the bridge logs and ignores them).
The on-board **`LED_BUILTIN`** blinks on each valid RX.

Map more nodes in [`include/gateway_config.h`](include/gateway_config.h); an
unknown `node_id` falls back to `water-node-<id>`.

### Build / flash

```
cd lora-gateway
pio run                 # first run pulls the STM32duino core (large, one-time)
pio run -t upload       # onboard ST-LINK (USB)
pio device monitor      # 115200 — you'll see the JSON lines directly
```

**Verified: compiles clean** — RAM 1.9%, Flash 9.8% of the STM32WL55JC.

## Bridge (the host forwarder) — `bridge/`

Standalone Node.js (kept **out** of `web-server/` so the server stays
hardware-agnostic, same as the AI poller). Reads the gateway's USB serial port
and POSTs each JSON line.

```
cd lora-gateway/bridge
npm install
cp .env.example .env        # set SERIAL_PORT (/dev/ttyACM0 or COMx) + SERVER_URL
npm run start:env           # loads .env via Node's --env-file
# or: SERIAL_PORT=/dev/ttyACM0 npm start
```

Config (env / `.env`):

| Var           | Default                                   | Notes                                  |
|---------------|-------------------------------------------|----------------------------------------|
| `SERIAL_PORT` | `/dev/ttyACM0`                            | gateway's ST-LINK VCP (Windows: `COMx`)|
| `SERIAL_BAUD` | `115200`                                  | matches the firmware                    |
| `SERVER_URL`  | `http://localhost:3000/api/v1/telemetry`  | on the Jetson the server is same-host   |
| `RECONNECT_MS`| `3000`                                    | serial reconnect backoff                |

The bridge auto-reconnects if the gateway is unplugged/replugged, and validates
`device_id` + `metrics` before POSTing. It needs **Node ≥ 18** (global `fetch`).

### Run it as a service (optional, survives reboot)

`systemd` unit sketch on the Jetson:

```ini
[Unit]
Description=LoRa gateway serial->HTTP bridge
After=network-online.target

[Service]
WorkingDirectory=/opt/smartfarm/lora-gateway/bridge
ExecStart=/usr/bin/node --env-file=.env bridge.js
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

## Status / caveats

- **Compiles clean, but not yet hardware-verified.** Same caveat as the node —
  `pio run` succeeds; validate a real RX + the bridge end-to-end before trusting it.
- The gateway must be on the **same LoRa channel/PHY** as the node (it is, via the
  shared `lora_params.h` copy) and reasonably close for SF9/125 kHz range.
- No auth on the serial→HTTP path; it assumes a trusted host (the Jetson).
