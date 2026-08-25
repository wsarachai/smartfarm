# water-temp-node

Battery-powered **hot/cold water temperature** LoRa sensor node on a
**NUCLEO-WL55JC1** (STM32WL55JC). Every 15 minutes it wakes from **Stop2**,
powers two **DS18B20** probes through an **A0341 P-MOSFET**, reads both
temperatures, measures the battery via the internal **VREFINT**, transmits one
compact **AS923** LoRa uplink, and goes back to sleep. It is **uplink-only** — it
never listens for a downlink.

It does **not** talk to the web-server directly (that server only speaks HTTP on
the WiFi LAN). The uplink is received by [`../lora-gateway`](../lora-gateway),
whose host [`bridge`](../lora-gateway/bridge) POSTs it to `/api/v1/telemetry`.

```
water-temp-node --LoRa AS923--> lora-gateway --USB CDC JSON--> bridge.js --HTTP--> web-server:3000
```

## Signal chain

```
RTC wake (Stop2, 15 min)
  -> gate ON  (P-MOSFET, active-low)  -> settle 10 ms
  -> DS18B20 x2 convert (12-bit, 750 ms) -> read hot + cold
  -> gate OFF -> park 1-Wire pins analog
  -> battery_mv via VREFINT
  -> pack 12-byte frame -> LoRa TX (923.2 MHz SF9BW125, 14 dBm)
  -> radio cold-sleep -> Stop2
```

## Hardware / wiring

Board: **NUCLEO-WL55JC1**. Powered from a **3.0–3.6 V** battery directly on the
3V3 domain (2×AA or 1× LiFePO4) — that's what makes the zero-part VREFINT battery
read valid. Radio uses the board's TCXO (DIO3-powered) and the on-board RF switch.

Pins (see [`include/node_config.h`](include/node_config.h) — change to match your
build). They deliberately avoid the RF-switch pins **PC3/PC4/PC5**, SWD
**PA13/PA14**, the TCXO **PB0**, and the VCP UART **PA2/PA3**:

| Function                         | Pin  |
|----------------------------------|------|
| DS18B20 **hot** data (1-Wire)    | PA10 |
| DS18B20 **cold** data (1-Wire)   | PA9  |
| Sensor-rail power gate (P-MOSFET)| PA8  |
| Debug log (LPUART1 → ST-LINK VCP)| PA2 (TX) / PA3 (RX) |

Power gate + probes (both probes **and** both pull-ups sit on the switched rail,
so there is zero leakage during sleep):

```
                3V3 ──S│ A0341 │D──┬──────────── DS18B20 rail
                       │ (P-ch)│   │
              100k ────┤gate   │   ├──[4.7k]──┬── DQ hot   (probe A, PA10)
             to 3V3    │       │   │          └── VDD hot
   (OFF when Hi-Z) ────┘       │   ├──[4.7k]──┬── DQ cold  (probe B, PA9)
                  PA8 ─────────┘   │          └── VDD cold
              (LOW = ON)           └── GND rail ── probe GNDs
```

`hot` vs `cold` is fixed by **which pin the probe is plugged into** — no ROM
addressing, so they can never get swapped in software.

## What goes over the air

A fixed **12-byte binary** frame (see
[`src/lora/lora_packet.h`](src/lora/lora_packet.h)): magic/version, node id, seq,
valid-flags, `temp_hot` + `temp_cold` (int16 centi-°C), `battery_mv` (uint16), and
a CRC-8. The gateway expands it to the server's `{device_id, metrics}` JSON and
adds `rssi`/`snr`. LoRa PHY (`src/lora/lora_params.h`) — **AS923 / 923.2 MHz /
SF9 / BW125 / CR4-5 / syncword 0x34-equiv / 14 dBm** — must stay identical to the
gateway's copy.

## Build / flash

```
cd water-temp-node
pio run                 # first run pulls the ststm32 platform + STM32WL HAL (slow, one-time)
pio run -t upload       # flash over the onboard ST-LINK (USB)
pio device monitor      # 115200 — shows the per-wake debug log
```

An **external ST-Link V2** works too (same `upload_protocol = stlink`): wire
SWDIO=PA13, SWCLK=PA14, NRST, GND, 3V3 and remove the onboard ST-LINK jumpers.

## Config knobs (`include/node_config.h`)

- `NODE_ID` — this node's wire id (the gateway maps it to `water-temp-01`).
- `WAKE_INTERVAL_S` — sleep interval (default `900`).
- `DS_*` pins, `DS_POWER_SETTLE_MS`, `DS_CONVERT_MS`.
- `DEBUG_UART_ENABLED` — comment out to drop serial logging entirely.

## Status / caveats

- **Not yet compiled or hardware-verified** (consistent with the sibling firmware
  in this repo). The first `pio run` pulls the toolchain; confirm the STM32WL HAL
  and `HAL_SUBGHZ_*` symbols resolve before trusting it.
- The LoRa driver ([`src/lora/subghz_lora.c`](src/lora/subghz_lora.c)) is a lean,
  self-contained SX126x command driver over ST's `HAL_SUBGHZ` — it does **not**
  vendor the full STM32CubeWL SubGHz_Phy middleware. RF-switch truth table and
  TCXO config are taken verbatim from ST's NUCLEO-WL55JC BSP.
- Advisory sensor only — this node never actuates anything.
