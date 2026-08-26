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
pio run                 # first run pulls the STM32duino core (large, one-time)
pio run -t upload       # flash over the onboard ST-LINK (USB)
pio device monitor      # 115200 — shows the per-wake debug log
```

**Verified: compiles clean** — RAM 2.3%, Flash 10.2% of the STM32WL55JC.

### Why `framework = arduino`, not `stm32cube`

PlatformIO's ststm32 platform has **no** STM32Cube framework for the STM32WL
family (no `framework-stm32cubewl` package exists — the `nucleo_wl55jc` board only
advertises `arduino`/`zephyr`). **STM32duino *is* the STM32Cube HAL underneath**,
so the HAL SubGHz driver, DS18B20, and battery code compile unchanged; we only
enable `HAL_SUBGHZ` via `include/hal_conf_extra.h` (STM32duino's HAL override
hook). Deep sleep (Stop2) + RTC wake are done with **raw HAL** in `main.cpp` — we
avoid the STM32duino Low Power/RTC libraries because their 2.0.0 releases
hard-`#error` against the core PlatformIO bundles.

An **external ST-Link V2** works too (same `upload_protocol = stlink`): wire
SWDIO=PA13, SWCLK=PA14, NRST, GND, 3V3 and remove the onboard ST-LINK jumpers.

## Config knobs (`include/node_config.h`)

- `NODE_ID` — this node's wire id (the gateway maps it to `water-temp-01`).
- `WAKE_INTERVAL_S` — sleep interval (default `900`).
- `DS_*` pins, `DS_POWER_SETTLE_MS`, `DS_CONVERT_MS`.
- `DEBUG_UART_ENABLED` — comment out to drop serial logging entirely.

## DS18B20 bring-up test on an STM32F103C8T6 (Blue Pill)

While the NUCLEO-WL55JC1 boards are on back-order, extra PlatformIO envs let you
validate the DS18B20 wiring + the **same `ds18b20.cpp` driver** the node ships, on
a cheap Blue Pill — no LoRa, no sleep, just the sensor. **Hardware-verified:** both
probes read real, independent room temperature (`hot=23.75 C  cold=23.87 C`).

Two envs, differing only in how the readings get off the board — both reuse
`src/ds18b20.cpp` unchanged (`build_src_filter` compiles only that driver +
`src/test/ds18b20_test_main.cpp`; the WL55 firmware is filtered out, and vice-versa):

| Env | Output path | When to use |
|-----|-------------|-------------|
| `bluepill_f103c8` | **USB CDC** (`Serial`, the board's micro-USB) | if the board's native USB enumerates |
| `bluepill_f103c8_semihosting` | **semihosting over SWD** (the ST-Link) | no working USB / UART / adapter |

Output (~1 Hz), either path:

```
DS18B20 F103 test | hot=PB6 cold=PB7 | direct 3V3, internal ~40k pull-up
hot=23.75 C  cold=23.87 C
hot=23.75 C  cold=FAULT(no presence)
```
(the banner reads `external 4.7k pull-up` when `DS18B20_INTERNAL_PULLUP` is off)

### Wiring (direct 3V3, **no** MOSFET gate — that's only for battery saving)

| Function                        | Blue Pill pin |
|---------------------------------|---------------|
| DS18B20 **hot** data            | PB6           |
| DS18B20 **cold** data           | PB7           |
| Heartbeat LED (onboard)         | PC13          |

Each probe: VDD→3V3, GND→GND, DQ→PB6/PB7, plus a **4.7 kΩ pull-up** DQ→3V3.
**No 4.7 kΩ on hand?** The `_semihosting` env sets `-D DS18B20_INTERNAL_PULLUP`,
which uses the MCU's internal ~40 kΩ pull-up instead — enough for a **short,
single-probe** bench bus (that's how the reads above were captured). Fit the real
4.7 kΩ and drop the flag for the WL55 node / any real cable length.

### Long DS18B20 runs (up to ~20 m)

The sensor can sit **10–20 m** from the MCU on plain 1-Wire (per Maxim/ADI
**AN148**) — you do **not** need RS-485 for this. In particular a **MAX485 cannot
go on the DS18B20 data line**: 1-Wire is bidirectional open-drain with the
direction reversing *inside each bit slot*, while a MAX485 is a directional
differential UART transceiver (needs `DE/RE` control) and the DS18B20 only speaks
single-ended 1-Wire. RS-485 only helps if a small MCU at the sensor end digitizes
the reading first and sends it as serial/Modbus (an architecture change, warranted
only past ~30 m).

For a reliable 10–20 m run:

1. **External pull-up ~2.2 kΩ** (1.5–3.3 kΩ) DQ→VDD **at the MCU end** — lower than
   4.7 kΩ so the line rises fast enough over the cable capacitance; keep ≥1.5 kΩ so
   the DS18B20 can still pull a valid low. **Do not** use the internal ~40 kΩ
   (`DS18B20_INTERNAL_PULLUP`) on a long run.
2. **Twisted pair: DQ twisted with GND** (its return); VDD on a separate conductor.
   Cat5/telephone cable, one pair = DQ+GND. Biggest single reliability factor.
3. **3-wire power (VDD)** — already how the node is wired; never parasitic on long lines.
4. Optional: ~**100 Ω series** at the MCU DQ pin (reflection damping) + **100 nF**
   across VDD/GND at the sensor end.
5. Each probe is its **own point-to-point line** (separate GPIO) — no stubs, the
   most forgiving topology.

```
MCU DQ ──[100Ω opt]──┬─────── twisted pair (DQ + GND) ─────── DQ (DS18B20)
       [2.2k]→VDD ────┘
VDD ───────────────────── separate conductor ──────────────── VDD (+100nF)
GND ───────────────────── (paired with DQ) ─────────────────── GND
```

**Firmware:** no timing change needed (standard-speed slots are valid to ~100 m).
When you fit the external pull-up, **remove `-D DS18B20_INTERNAL_PULLUP`** from the
node env so `ds18b20_init()` uses `GPIO_NOPULL` + your resistor. If passive 2.2 kΩ
proves marginal past ~20–30 m, escalate to a **DS2483/DS2482** (I²C→1-Wire) or
**DS2480B** (UART→1-Wire) line-driver IC with hardware active pull-up.

### Option A — USB CDC (`bluepill_f103c8`)

```
pio run -e bluepill_f103c8 -t upload     # ST-Link V2 on the SWD header
pio device monitor -e bluepill_f103c8    # the board's micro-USB COM port
```

**USB caveat:** many Blue Pill *clones* have a wrong USB pull-up (R10 = 10 kΩ
instead of 1.5 kΩ) and never enumerate — no COM port appears at all. Fix with a
1.5 kΩ from PA12→3V3, or just use the semihosting env below (no USB needed).

### Option B — semihosting over SWD (`bluepill_f103c8_semihosting`)

Prints through the **ST-Link/SWD** you already use to flash — no USB, UART, or
adapter. The output shows only while a **debugger is attached** (the board's
`BKPT` faults standalone), so this is a bench-viewing mode.

**CLI:**
```
pio run -e bluepill_f103c8_semihosting -t upload
# then stream the readings (Ctrl+C to stop):
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "init" -c "arm semihosting enable" -c "reset run"
```
(OpenOCD ships with PlatformIO at
`~/.platformio/packages/tool-openocd/bin/openocd` — pass its
`.../openocd/scripts` dir via `-s` if run from outside PlatformIO.)

**VSCode (PlatformIO extension):**
1. Bottom **status bar** → switch the active env to `env:bluepill_f103c8_semihosting`.
2. **Run → Start Debugging** (**F5**). It builds, flashes, and the env's
   `debug_extra_cmds = monitor arm semihosting enable` turns semihosting on.
3. It halts at `main()` — press **Continue (F5)**.
4. Readings stream into the **Debug Console** (`Ctrl+Shift+Y`); if empty, check the
   OpenOCD **Terminal** tab. Stop with the ■ button.

> Use **Debug (F5)**, not the Upload/Monitor buttons — the Monitor button grabs a
> serial COM port (semihosting isn't one), so it shows nothing here.

### Driver note
`ds18b20_read()` rejects an all-zero scratchpad: a data line stuck low (missing
pull-up / short) returns all zeros, which passes CRC (`crc8(0..0)=0`) and would
otherwise decode to a **fake 0.00 °C**. It now reports a fault instead — this
guard benefits the WL55 node too.

## Status / caveats

- **Compiles clean, but not yet hardware-verified.** `pio run` succeeds (RAM
  2.3%, Flash 10.2%); the radio, sleep current, and DS18B20 timing still need a
  real board. Do the first flash + monitor before trusting it.
- The LoRa driver ([`src/lora/subghz_lora.c`](src/lora/subghz_lora.c)) is a lean,
  self-contained SX126x command driver over `HAL_SUBGHZ` — it does **not** vendor
  the full STM32CubeWL SubGHz_Phy middleware. RF-switch truth table and TCXO
  config are taken verbatim from ST's NUCLEO-WL55JC BSP.
- Advisory sensor only — this node never actuates anything.
