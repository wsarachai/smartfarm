# water-temp-node

Battery-powered **water + air** LoRa sensor node on a **NUCLEO-WL55JC1**
(STM32WL55JC). Every 15 minutes it wakes from **Stop2**, powers the sensor rail
through an **A0341 P-MOSFET**, reads **hot/cold water temperature** (2× DS18B20),
**air temperature + humidity** (SHT45) and **CO2** (SCD41), measures the battery
via the internal **VREFINT**, transmits one compact **AS923** LoRa uplink, and
goes back to sleep. It is **uplink-only** — it never listens for a downlink.

All four sensors sit on the **one gated rail**, so the whole front-end is dead
between wakes. The SCD41 is the expensive one and is paced separately — see
[CO2 on a battery node](#co2-on-a-battery-node).

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
  -> DS18B20 x2 start convert (12-bit, 750 ms)
  -> wait out the SCD41's 1 s power-up   <- covers the 750 ms conversion,
  |                                         so the rail is on for the LONGER
  |                                         of the two, not the sum
  -> read hot + cold
  -> SHT45  read (~10 ms)              -> air temp + humidity
  -> SCD41  single shot (5 s, x2 with warm-up)  -> CO2   [every Nth wake]
  -> gate OFF -> park 1-Wire + I2C pins analog
  -> battery_mv via VREFINT
  -> pack 20-byte v3 frame -> LoRa TX (923.2 MHz SF9BW125, 14 dBm)
  -> radio cold-sleep -> Stop2
```

## Hardware / wiring

> Building the **real hardware**? It is two PCBs — the NUCLEO-WL55JC1 plus a
> custom **ARDUINO Uno V3 shield** carrying the probe connectors, the SHT45 +
> SCD41, the P-MOSFET gate, protection and the battery input. See
> [`docs/hardware-interface.md`](docs/hardware-interface.md) for the full
> CN6/CN8/CN9/CN5 pin map, the eight signals crossing the joint, the schematic +
> BOM, the Nucleo battery traps, and the bring-up order. The wiring below is the
> bench setup.

Board: **NUCLEO-WL55JC1**. Powered from a **3.0–3.6 V** battery directly on the
3V3 domain (2×AA or 1× LiFePO4) — that's what makes the zero-part VREFINT battery
read valid. Radio uses the board's TCXO (DIO3-powered) and the on-board RF switch.

Pins (see [`include/node_config.h`](include/node_config.h) — change to match your
build). They deliberately avoid the RF-switch pins **PC3/PC4/PC5**, SWD
**PA13/PA14**, the TCXO **PB0**, and the VCP UART **PA2/PA3**:

| Function                            | Pin  | ARDUINO |
|-------------------------------------|------|---------|
| DS18B20 **hot** data (1-Wire)       | PA10 | CN8-3 (A2) |
| DS18B20 **cold** data (1-Wire)      | PA9  | CN5-2 (D9) |
| **SDA** (SHT45 + SCD41)             | PA11 | CN5-9 (D14) |
| **SCL** (SHT45 + SCD41)             | PA12 | CN5-10 (D15) |
| Sensor-rail power gate (P-MOSFET)   | PB2  | CN8-2 (A1) |
| Battery in (3V3)                    | —    | CN6-4 |
| Debug log (LPUART1 → ST-LINK VCP)   | PA2 (TX) / PA3 (RX) | CN9-7/8 |

Every pin is on the **ARDUINO Uno V3 headers**, because the real front-end is a
**shield** — see [`docs/hardware-interface.md`](docs/hardware-interface.md). That
constraint is why the gate is **PB2 and not PA8**: PA8 is one of the few MCU pins
the Nucleo brings out only to the morpho headers, so a shield cannot reach it.

PA11/PA12 are the board's designated ARDUINO I2C pair. The alternatives were
worse: **PA9/PA10** are the DS18B20 probes, and **PB10/PB11** would put SDA on the
Nucleo's LED3.

Power gate + sensors. **Everything** — both probes, both 1-Wire pull-ups, the two
I2C parts and the I2C pull-ups — sits on the switched rail, so there is zero
leakage during sleep:

```
                3V3 ──S│ A0341 │D──┬──────────── VSENS (switched rail)
                       │ (P-ch)│   │
              100k ────┤gate   │   ├──[4.7k]──┬── DQ hot   (probe A, PA10)
             to 3V3    │       │   │          └── VDD hot
   (OFF when Hi-Z) ────┘       │   ├──[4.7k]──┬── DQ cold  (probe B, PA9)
                  PB2 ─────────┘   │          └── VDD cold
              (LOW = ON)           │
                                   ├──[4.7k]───── SDA (PA11) ─┬─ SHT45  (0x44)
                                   ├──[4.7k]───── SCL (PA12) ─┤
                                   ├───────────── VDD ────────┴─ SCD41  (0x62)
                                   └── GND rail ── probe + sensor GNDs
```

`hot` vs `cold` is fixed by **which pin the probe is plugged into** — no ROM
addressing, so they can never get swapped in software. The SHT45 and SCD41 have
fixed, distinct addresses, so they share one bus with no strapping.

> The SCD41 draws **~205 mA peaks**. Size the rail bulk capacitance and the
> P-MOSFET for that, not for the DS18B20's few mA — and see the settle-time
> constraint in [`docs/hardware-interface.md`](docs/hardware-interface.md),
> because more bulk fights the 10 ms `DS_POWER_SETTLE_MS`.

## What goes over the air

A **20-byte binary** frame, **v3** (see
[`src/lora/lora_packet.h`](src/lora/lora_packet.h)):

| Bytes | Field | Notes |
|---|---|---|
| 0 | magic | `0xA3` = v3. Rejects noise and older/newer frames cheaply |
| 1–3 | node id, seq, flags | `flags` says which fields below are valid |
| 4–7 | `temp_hot`, `temp_cold` | int16 centi-°C; sentinel `0x8000` if the probe failed |
| 8–9 | `battery_mv` | uint16 mV |
| 10–13 | `air_temp`, `humidity` | int16 centi-°C, uint16 %RH ×100 — **from the SHT45** |
| 14–15 | `pressure` | uint16 deci-hPa; **0 on this node** (no barometer) |
| 16–17 | `co2` | uint16 ppm |
| 18–19 | reserved, CRC-8 | CRC over bytes 0..18 |

The gateway expands it to the server's `{device_id, metrics}` JSON and adds
`rssi`/`snr`. LoRa PHY (`src/lora/lora_params.h`) — **AS923 / 923.2 MHz / SF9 /
BW125 / CR4-5 / syncword 0x34-equiv / 14 dBm** — must stay identical to the
gateway's copy.

**Versioning:** v1 (12 B, water temps + battery) and v2 (18 B, + BME280 ambient)
are still on the wire from other builds and are **unchanged**. Each version only
appends, so a field never moves, and `lora_packet_unpack()` accepts all three —
an older node in the field keeps working against a current gateway.

`LORA_FLAG_SHT` (bit 7) says air temp/humidity came from an **SHT45** rather than
a BME280. Same units either way, so the receiver need not branch on it; it exists
so a swapped sensor is visible in the logs rather than silently changing what the
numbers mean.

## CO2 on a battery node

The SCD41 is, by a wide margin, the most expensive thing on this board — worth
understanding before trusting the battery life estimate.

A single-shot measurement **blocks for 5 s**, and the datasheet is explicit that
the **first** shot after power-up is unsettled, so an honest reading costs **two**
— about **10 s** of sensor-on time. Compare that with the rest of a wake, which is
under a second, and with the LoRa TX, which is milliseconds.

Two knobs in [`include/node_config.h`](include/node_config.h) control the cost:

| Knob | Default | Effect |
|---|---|---|
| `CO2_ENABLED` | on | Comment out to drop the SCD41 entirely |
| `CO2_EVERY_N_WAKES` | `4` | Measure CO2 on every Nth wake only |
| `CO2_SINGLE_SHOT_WARMUP` | `1` | Discard one shot first. Halving the cost by turning this off also makes the readings worse |

At the default `WAKE_INTERVAL_S = 900`, `CO2_EVERY_N_WAKES = 4` gives **one CO2
reading per hour** and puts the sensor on for 10 s in every 3600 — a **0.28 % duty
cycle** rather than the 1.1 % of measuring every wake. A greenhouse CO2 trend does
not move faster than that. On the wakes in between, the **last reading is
re-sent** with its valid flag still set: the number is real, just up to an hour
old.

To turn duty cycle into milliamp-hours you need the measurement current for your
part — take it from the SCD4x datasheet rather than from this README, since it
differs between revisions and supply voltages. The shape of the arithmetic is
`I_avg ≈ I_measure × duty + I_idle`, and the point of the power gate is to make
that second term the MOSFET's leakage rather than the sensor's idle draw.

**Why single-shot here and periodic on the F103 prototype:** periodic mode is more
accurate (the cell stays warm) but costs milliamps continuously, which is fine for
a USB-powered bench board and fatal for a battery. The driver supports both; the
host picks.

## Build / flash

```
cd water-temp-node
pio run                 # first run pulls the STM32duino core (large, one-time)
pio run -t upload       # flash over the onboard ST-LINK (USB)
pio device monitor      # 115200 — shows the per-wake debug log
```

**Verified: compiles clean** — RAM 3.1%, Flash 15.4% of the STM32WL55JC.

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
- `I2C_SDA_PIN` / `I2C_SCL_PIN` / `I2C_CLOCK_HZ`, `SHT45_ADDR`, `SCD41_ADDR`.
- `I2C_POWER_SETTLE_MS` — the SCD41's mandatory 1 s wait after the rail comes up.
- `CO2_ENABLED`, `CO2_EVERY_N_WAKES`, `CO2_SINGLE_SHOT_WARMUP` — see
  [CO2 on a battery node](#co2-on-a-battery-node).
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
DS18B20 F103 test | hot=PB6 cold=PB7 | direct 3V3, external 4.7k pull-up
hot=23.62 C  cold=23.18 C
hot=23.62 C  cold=FAULT(no presence)
```
(the banner always states which pull-up the running build configured, so it can
never silently disagree with the wiring — see the switch below)

### Diagnosing a bad bus (`bluepill_f103c8_dump`)

When the test prints `FAULT(crc)` or a suspicious `0.00 C`, flash the dump env
instead: it probes each DQ pin electrically at boot, then prints the **raw 9-byte
scratchpad** + CRC verdict per probe (same pins, semihosting over SWD).

```
pio run -e bluepill_f103c8_dump -t upload
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c init -c "arm semihosting enable" -c "reset run"
```

Reading the output:

| Line | Meaning |
|------|---------|
| `pull-up=1 pull-down=1` | a strong external pull-up (4.7 kΩ) is winning — wiring OK |
| `pull-up=1 pull-down=0` | **no** external pull-up; only the MCU's internal ~40 kΩ |
| `pull-up=0 pull-down=0` | DQ **held low** by something stronger than 40 kΩ — a short to GND, or the pull-up resistor tied to the GND rail instead of 3V3 |
| `FF FF FF …` | nothing drives the line — probe absent / DQ not connected |
| `00 00 00 …` | line stuck low (the fake-`0.00 C` case `ds18b20_read()` rejects) |
| `LO HI 4B 46 7F FF 0C 10 CRC` | a healthy 12-bit probe |

### Wiring (direct 3V3, **no** MOSFET gate — that's only for battery saving)

| Function                        | Blue Pill pin |
|---------------------------------|---------------|
| DS18B20 **hot** data            | PB6           |
| DS18B20 **cold** data           | PB7           |
| Heartbeat LED (onboard)         | PC13          |

Each probe: VDD→3V3, GND→GND, DQ→PB6/PB7, plus a pull-up on each DQ line.

**As wired today: an external 4.7 kΩ per line, DQ→3V3** — hardware-verified reading
both probes, with the dump env reporting `pull-up=1 pull-down=1` on both (the 4.7 kΩ
beating the MCU's internal 40 kΩ pull-down is the proof the resistor really is on
3V3). Mind that polarity: tying it to GND instead holds the bus low and every read
comes back all-zero (see *Diagnosing a bad bus* above).

### The pull-up switch

The pull-up is a fact about the **wiring**, so it lives in **one** place —
`ds18b20_pullup` in `[bluepill_base]` — and every F103 env interpolates it. Change
it there and reflash; no env needs touching:

| `ds18b20_pullup =` | Wiring it matches |
|---|---|
| *(empty)* | external 4.7 kΩ DQ→3V3 per probe; `ds18b20_init()` uses `GPIO_NOPULL` **← current** |
| `-D DS18B20_INTERNAL_PULLUP` | no resistor; the MCU's internal ~40 kΩ. Weaker, so a **short** bench bus only — never the WL55 node or a real cable run |

The WL55 env is separate and never sets the flag: the real node always has its
external resistors.

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
   (`DS18B20_INTERNAL_PULLUP`) on a long run — it is a bench-only shortcut.
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

### The full prototype node (`bluepill_f103c8_node`)

Beyond the DS18B20-only bring-up envs, two envs run the **whole node** on the
Blue Pill — every sensor, the shared v3 frame, and an external **SX1278 at
433 MHz** talking to [`../lora-pi-receiver`](../lora-pi-receiver). This is where
the SHT45 and SCD41 drivers get their real-hardware hours before the WL55 boards
arrive, since both envs build the **same driver files** the WL55 node ships
(`build_src_filter`, no copies).

| Env | Output | Use |
|---|---|---|
| `bluepill_f103c8_node` | semihosting over SWD | Bench, with a debugger attached |
| `bluepill_f103c8_node_standalone` | none (silent) | Flash once, run from any USB/battery supply |

```
pio run -e bluepill_f103c8_node -t upload
```

Wiring beyond the DS18B20 probes (PB6/PB7):

| Part | Bus | Address | Supplies |
|---|---|---|---|
| BME280 | I2C2 PB10=SCL / PB11=SDA | `0x76` | `pressure` **only** |
| SHT45 | same bus | `0x44` | `air_temp` + `humidity` |
| SCD41 | same bus | `0x62` | `co2` |
| SX1278 | SPI1 | — | NSS=PA4 SCK=PA5 MISO=PA6 MOSI=PA7 RESET=PB0 DIO0=PB1 |

One pair of 4.7 k pull-ups serves the whole I2C bus. I2C**2** is used because
I2C1's PB6/PB7 are the DS18B20 probes.

Two behaviours differ from the WL55 node, deliberately:

- **The SCD41 runs in periodic mode** (free-running, one sample per 5 s) rather
  than single-shot. This board is mains-powered and awake continuously, so
  periodic is both simpler and more accurate — the cell stays warm.
- **The SHT45 supersedes the BME280 for temp/RH** (±0.1 °C / ±1 %RH beats it on
  both), and `LORA_FLAG_SHT` is set to record that. If the SHT45 does not answer,
  the BME280 takes those two fields back and the flag clears — a dead SHT45 costs
  accuracy, not telemetry.

The measured pressure is fed back to the SCD41 (`scd41_set_ambient_pressure`) for
CO2 density compensation, which is worth roughly 1 % of reading per 10 hPa.

> **Power:** the SCD41 peaks at ~205 mA. A marginal 3V3 LDO browns out
> mid-measurement, and the symptom looks like a flaky I2C bus rather than a power
> problem. Use a supply with headroom.

### Driver note
`ds18b20_read()` rejects an all-zero scratchpad: a data line stuck low (missing
pull-up / short) returns all zeros, which passes CRC (`crc8(0..0)=0`) and would
otherwise decode to a **fake 0.00 °C**. It now reports a fault instead — this
guard benefits the WL55 node too.

## Status / caveats

- **Compiles clean, but not yet hardware-verified.** `pio run` succeeds (RAM
  3.1%, Flash 15.4%); the radio, sleep current, and DS18B20 timing still need a
  real board. Do the first flash + monitor before trusting it.
- **The SHT45 and SCD41 are compile-verified only on this board.** Both drivers
  are hand-rolled from the datasheets against the shared
  [`sensirion_i2c`](src/sensirion_i2c.h) helper — no vendor library — and the
  wire format is cross-checked end to end (see below), but neither part has been
  on a WL55 yet. Bring them up on the F103 prototype env first, where the same
  driver files run against real sensors.
- **The frame format IS verified**, in both directions: the C header is compiled
  on the host to generate wire vectors, and
  [`lora-pi-receiver/test_lora_packet.py`](../lora-pi-receiver/test_lora_packet.py)
  decodes them, covering v1/v2/v3 plus every single-byte corruption. Run it after
  any change to the wire format.
- The LoRa driver ([`src/lora/subghz_lora.c`](src/lora/subghz_lora.c)) is a lean,
  self-contained SX126x command driver over `HAL_SUBGHZ` — it does **not** vendor
  the full STM32CubeWL SubGHz_Phy middleware. RF-switch truth table and TCXO
  config are taken verbatim from ST's NUCLEO-WL55JC BSP.
- Advisory sensor only — this node never actuates anything.
