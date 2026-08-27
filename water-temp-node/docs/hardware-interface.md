# water-temp-node — hardware interface spec

The real (non-breadboard) build is **two PCBs joined by a connector**:

| Board | What it is | Carries |
|---|---|---|
| **Brain** | NUCLEO-WL55JC1 (off-the-shelf) | STM32WL55JC, radio, RF switch, TCXO, antenna, ST-LINK |
| **Front-end** | Custom shield PCB, this spec | DS18B20 probe connectors, SHT45 + SCD41 (I2C), A0341 P-MOSFET rail gate, pull-ups, line protection, battery input |

The front-end is an **ARDUINO Uno V3 shield** that mounts above the Nucleo on its
ARDUINO headers. This document specifies that joint: the connectors, the signals
crossing them, and the front-end circuit behind them.

Everything here is a contract the **firmware already assumes**. Where a value is
forced by code, the code is cited — change one and you change the other.

- Firmware pin map: [`include/node_config.h`](../include/node_config.h)
- Gate + sleep handling: [`src/main.cpp`](../src/main.cpp)
- Cable-length guidance: [README → *Long DS18B20 runs*](../README.md)

---

## 1. The stack

The front-end is an **ARDUINO Uno V3 shield**. It mates with the Nucleo's four
ARDUINO headers — **CN6, CN8, CN9, CN5** — and leaves the ST morpho headers
(CN7/CN10) free for probing.

```
        ┌─────────────────────────────────────────────┐
        │  FRONT-END SHIELD                           │
        │  probe J1/J2 · gate Q1 · pull-ups · TVS     │   <- this spec
        │  SHT45 U1 · SCD41 U2 (I2C) · battery J3     │
        │                                             │
        │  [CN6 8p][CN8 6p]        [CN9 8p][CN5 10p]  │
        │   BATT    DQ_HOT           —      DQ_COLD   │
        │   GND     SENS_GATE               SDA/SCL   │
        └────┬─────────┬───────────────┬────────┬─────┘
             │         │               │        │       2.54 mm
        ┌────┴─────────┴───────────────┴────────┴─────┐
        │  NUCLEO-WL55JC1     [antenna keep-out]      │
        │  (ST-LINK section snapped off — see §5)     │
        │  morpho CN7/CN10 remain exposed → Appendix A│
        └─────────────────────────────────────────────┘
```

- **Mating:** the Nucleo's ARDUINO connectors are **female**, so the shield
  carries **male** pin headers on its underside — the opposite gender to a
  morpho-stacking board. Lengths are fixed by the Uno V3 standard: **CN6 = 8,
  CN8 = 6, CN9 = 8, CN5 = 10** positions.
- **Mechanical:** headers on both long edges support the board at both edges — no
  standoff strictly required, but fit **M3 nylon standoffs** at the Nucleo's
  mounting holes anyway. A board held only by header friction will fret its
  contacts loose under vibration and thermal cycling.
- **Stack height:** the headers must clear the Nucleo's tallest parts (USB
  connector, antenna connector). Use tall/stacking headers or add spacers, and
  confirm against your actual board before ordering.
- **Antenna keep-out:** cut the shield away over the Nucleo's RF section and the
  **CN12 SMA** connector. No copper, no battery, no probe cable routed over it.
  The Uno V3 outline is **fixed**, so unlike a custom morpho board you cannot
  simply move the board edge to dodge the RF section — **check the keep-out
  against a physical board before laying out copper.** This is the one place the
  shield choice costs you freedom.

### Keying — solved for free by the footprint

This used to be the most dangerous paragraph in the document. It no longer is,
and that is the main reason to prefer the shield.

Two identical 2×19 morpho headers are symmetric about the board centre, so a
stacking board will mate **rotated 180°** just as happily as the right way round —
putting the battery onto a signal pin. Guarding against that needed a deliberate
key.

The ARDUINO Uno V3 footprint **cannot be fitted wrong**. Its four connectors have
different lengths (8/6/8/10), and the classic **0.16″ (not 0.1″) offset between
D7 and D8** makes the two long edges non-interchangeable. Rotate it and nothing
lines up; mirror it and nothing lines up. The geometry is the key.

Still worth doing, at no cost:

- **Silkscreen pin-1 triangles** on the shield plus a printed arrow, so a
  half-inserted board is obvious.
- **Mark the header lengths** (`CN6·8  CN8·6  CN9·8  CN5·10`) next to each strip.

---


---

## 2. Signals crossing the connector

**Eight** signals. Freeze this table; it is the interface. Every other position on
the ARDUINO headers is **left unconnected** on the shield — do not casually route
extra pins "in case", each one is a new way to fight the debugger.

| Signal | WL55 pin | ARDUINO | Direction | Electrical | Forced by |
|---|---|---|---|---|---|
| `VBAT` | — | **CN6-4** (`3V3`) | in to brain | 3.0–3.6 V, ≤250 mA peak | `battery.cpp` — battery **is** VDDA |
| `GND` | — | **CN6-6/7**, **CN5-7** | — | use **≥2 GND pins**, one per edge | — |
| `SENS_GATE` | **PB2** | **CN8-2** (A1) | out of brain, push-pull | active **LOW** = rail on; Hi-Z = off via 100 k | `node_config.h` `DS_PWR_*` |
| `DQ_HOT` | **PA10** | **CN8-3** (A2) | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_HOT_*` |
| `DQ_COLD` | **PA9** | **CN5-2** (D9) | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_COLD_*` |
| `I2C_SDA` | **PA11** | **CN5-9** (D14) | bidir, open-drain | I2C @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SDA_PIN` |
| `I2C_SCL` | **PA12** | **CN5-10** (D15) | out of brain, open-drain | I2C @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SCL_PIN` |
| `VSENS` | — | — | front-end internal | gated 3V3 to probes, I2C parts **and** all pull-ups | `main.cpp` `gate_on()` |

The peak figure is the **SCD41's ~205 mA measurement**, not the LoRa TX's ~150 mA:
the firmware gates the sensor rail **off** before it powers the radio, so the two
never overlap. Size `VBAT` for the larger one, not their sum.

`VSENS` is generated **on the front-end** and never crosses back to the Nucleo.
The only power crossing the joint is `VBAT` and ground.

Not part of the contract: the LPUART1 VCP (PA2/PA3, at CN9-7/8). SWD is **not**
on these headers at all — see "Do not route these".

> **Two revisions of this interface exist and they are not compatible.** The
> original was a morpho-stacking board with six signals; it then grew to eight
> (SHT45 + SCD41), and it is now an **ARDUINO shield** with `SENS_GATE` moved from
> **PA8 to PB2**. PA8 is unreachable from a shield, which is what forced the move.
> Boards fabricated to either earlier revision must be rebuilt, not adapted.

### Two rules the front-end must honor

**1. The pull-ups sit on `VSENS`, never on permanent 3V3.** This now covers
**four** pull-ups — two 1-Wire and two I2C. On the always-on rail, each gated-off
part becomes a leakage path through its clamp diodes and the 15-minute duty cycle
stops meaning anything. `sensor_pins_park()` in `main.cpp` parks all four pins as
analog for the same reason — the board has to cooperate.

**2. `SENS_GATE` is active-low with a 100 k gate→source pull-up.** That resistor is
not optional: it holds the sensor rail **off** while PB2 is Hi-Z — during reset,
during BOOT0, and before firmware runs.

### Pin choice

Every signal above is reachable on the ARDUINO headers — the constraint that set
the whole map, and the reason `SENS_GATE` is PB2 (see below). PB2/PA9/PA10/PA11/
PA12 also avoid PC3/PC4/PC5 (RF switch), PA13/PA14 (SWD) and PB0 (TCXO), though
with a shield that is automatic: none of those reach these headers.

**PA11/PA12 are the board's designated ARDUINO I2C pair** (D14/SDA, D15/SCL), so
the shield gets I2C exactly where a shield expects it — which also means an
off-the-shelf Qwiic/Grove proto-shield works for bench bring-up. The alternatives
were worse: **PA9/PA10** are the DS18B20 probes, and **PB10/PB11** put SDA on the
Nucleo's **LED3**, loading the bus and burning current on every transfer.

> **One label to verify.** UM2592 Table 17 calls PA11/PA12 `I2C1_SDA`/`I2C1_SCL`,
> but ST's own STM32duino `PeripheralPins.c` and Zephyr's board definition both
> map them to **I2C2**. The firmware is unaffected either way — STM32duino
> resolves the instance from the pin map, not from a name — but if you need the
> instance for a bare-HAL port, check RM0453's alternate-function table rather
> than trusting either source.

### The ARDUINO pin map

Transcribed from **UM2592 Rev 1, Table 17** ("ARDUINO connectors pinout"). Only
**24** of the MCU's I/Os reach these headers — the full 76-position morpho map is
in [Appendix A](#appendix-a--st-morpho-pin-map-not-used-by-this-design) for bench
probing. Signals this node uses are **bold** and marked ✅; ⛔ marks positions the
shield must not touch. Everything unmarked is left open.

**CN6 — power (8 pos)** · same edge as CN8

| Pin | Name | MCU | Use |
|---:|---|---|---|
| 1 | NC | — | reserved for test |
| 2 | IOREF | — | |
| 3 | NRST | NRST | |
| 4 | **3V3** | — | ✅ **battery in** |
| 5 | 5V | — | |
| 6 | **GND** | — | ✅ use |
| 7 | **GND** | — | ✅ use |
| 8 | VIN | — | 7–12 V input, unused |

**CN8 — analog (6 pos)** · same edge as CN6

| Pin | Name | MCU | Use |
|---:|---|---|---|
| 1 | A0 | PB1 | |
| 2 | **A1** | **PB2** | ✅ `SENS_GATE` |
| 3 | **A2** | **PA10** | ✅ `DQ_HOT` |
| 4 | A3 | PB4 | |
| 5 | A4 | PB14 | |
| 6 | A5 | PB13 | |

**CN9 — digital low (8 pos)** · opposite edge, unused by this design

| Pin | Name | MCU | Use |
|---:|---|---|---|
| 1 | D7 | PC1 | |
| 2 | D6 | PB10 | |
| 3 | D5 | PB8 | |
| 4 | D4 | PB5 | |
| 5 | D3 | PB3 | ⚠️ also TRACESWO |
| 6 | D2 | PB12 | |
| 7 | D1 / TX | PA2 / PB6 | ⛔ ST-LINK VCP |
| 8 | D0 / RX | PA3 / PB7 | ⛔ ST-LINK VCP |

**CN5 — digital high (10 pos)** · same edge as CN9

| Pin | Name | MCU | Use |
|---:|---|---|---|
| 1 | D8 | PC2 | |
| 2 | **D9** | **PA9** | ✅ `DQ_COLD` |
| 3 | D10 | PA4 | |
| 4 | D11 | PA7 | |
| 5 | D12 | PA6 | |
| 6 | D13 | PA5 | |
| 7 | **GND** | — | ✅ use — the return for this edge |
| 8 | AVDD | AVDD/VREF+ | ⛔ voltage reference, do not load |
| 9 | **D14 / SDA** | **PA11** | ✅ `I2C_SDA` |
| 10 | **D15 / SCL** | **PA12** | ✅ `I2C_SCL` |

So the eight contract signals land like this:

| Signal | MCU pin | Connector | Pin |
|---|---|---|---|
| `VBAT` (battery in) | — | **CN6** | **4** (`3V3`) |
| `GND` | — | **CN6** | **6**, **7** |
| `GND` | — | **CN5** | **7** |
| `SENS_GATE` | PB2 | **CN8** | **2** (A1) |
| `DQ_HOT` | PA10 | **CN8** | **3** (A2) |
| `DQ_COLD` | PA9 | **CN5** | **2** (D9) |
| `I2C_SDA` | PA11 | **CN5** | **9** |
| `I2C_SCL` | PA12 | **CN5** | **10** |
| `VSENS` | — | — | front-end internal, does not cross |

The split falls out cleanly along the two edges:

- **CN6 + CN8 edge — power and the gate.** Battery in, both grounds, the P-MOSFET
  gate, and `DQ_HOT`. Put Q1, its 100 k, the bulk cap and the battery connector
  here; the 205 mA switching loop then never leaves this corner.
- **CN5 edge — the sensing.** `DQ_COLD`, both I2C lines, and a local GND return
  at CN5-7. Put the SHT45, the SCD41 and the I2C pull-ups here.

Only `DQ_HOT` crosses over, and that is unavoidable — PA10 is on CN8 and PA9 is
on CN5. Run it as a guarded trace, not under the SCD41.

### Why `SENS_GATE` is PB2 and not PA8

**PA8 is not on the ARDUINO headers.** It is one of the pins the Nucleo brings
out only to morpho CN10-16 (UM2592 Table 17 lists 24 MCU I/Os; PA8 is not among
them). A shield physically cannot reach it, so the gate had to move.

**PB2 (A1, CN8-2)** was chosen because:

- It is on the **CN6/CN8 edge**, next to where the battery arrives, so the whole
  high-current gate circuit stays in one corner.
- Its only alternate function is **ADC1_IN4** — nothing this design uses.
- The STM32WL has **no boot strap here**: BOOT0 is PH3, and nBOOT1 is an option
  byte rather than a pin. It resets to a floating input, which is exactly what the
  100 k gate pull-up needs to hold the rail off before firmware runs.

Runner-up was PB12 (D2, CN9-6), whose function UM2592 lists as plain "IO" — the
cleanest pin on the board, but on the wrong edge.

### Do not route these

Choosing a shield **removed most of this section**, which is a real benefit: the
RF-critical pins are not on the ARDUINO headers at all, so no copper pour on the
front-end can reach them.

| Position | Signal | Why |
|---|---|---|
| CN9-7, CN9-8 | PA2/PB6, PA3/PB7 | **ST-LINK VCP** — the debug log |
| CN5-8 | AVDD / VREF+ | **Voltage reference.** Loading it skews `battery_read_mv()` |
| CN9-5 | PB3 | Also **TRACESWO**; avoid if you ever want trace |
| — | PC3, PC4, PC5 | **RF switch** FE_CTRL1/2/3 — *not reachable from a shield* ✅ |
| — | PB0 | **VDD_TCXO** — *not reachable from a shield* ✅ |
| — | PA13, PA14 | **SWD** — *not reachable from a shield* ✅ |

The flip side: because SWD is not on these headers, the shield cannot carry debug
out to a test point. Use the Nucleo's own connector, or morpho CN7-13/15.

### Orientation — the headers are on the underside

The shield's pin headers face **down**. Everything above is written as the
Nucleo's own numbering, which is what you read off the board component-side up.
On the shield's layout those same positions **mirror**:

```
   Nucleo, viewed from above        Shield, viewed from above
   (female headers, component side) (male headers, on the underside)

   CN6 CN8            CN9 CN5       CN5 CN9            CN8 CN6
   [1]                        [1]   [1]                        [1]
   [2]                        [2]   [2]                        [2]
    …                          …     …                          …
```

Get this wrong and every signal is off by one position along the strip. Lay the
footprint out from the **mating** view and have someone else check it against the
tables above. Unlike the morpho version, a mirrored shield will at least not put
battery voltage onto a GPIO — the outline stops it mating at all — but it is
still a board spin.

---


---

## 3. Sensor front-end circuit

Two independent sensing domains share the one gated rail: the **1-Wire probes**
(off-board, on cable) and the **I2C air sensors** (on-board). They have nothing
in common electrically beyond `VSENS` and GND.

### The 1-Wire probes

One point-to-point line per probe — no shared bus, which is what makes the
driver's SKIP ROM legal. Both channels identical:

```
                    ┌──────── VSENS (switched) ────────┬─────────────┐
                    │                                  │             │
                  [2.2k]                             [100nF]      to probe
                    │                                  │          VDD pin
 CN8-3 ──[100R]────┼──────────────────────────────────┼────────── DQ
                    │                                  │
                   TVS                                GND ───────── GND
              (low-C, bidir)                      (twisted with DQ)
                    │
                   GND
```

### The I2C sensors

Both parts mount **on the front-end board**, not on a cable — they measure the
enclosure's air, and a long I2C run outside the box would be a fifth thing to go
wrong. Fixed, distinct addresses mean no strapping resistors:

```
        ┌──── VSENS (switched) ────┬──────────────┬──────────────┐
        │                          │              │              │
     [4.7k]  [4.7k]            [100nF]        [100nF]        [10uF]
        │       │                  │              │              │
  CN5-9 ┼───────┼──── SDA ─────────┤ U1 SHT45     ┤ U2 SCD41     │
  (PA11)│       │                  │   (0x44)     │   (0x62)     │
 CN5-10 ────────┼──── SCL ─────────┤              ┤              │
  (PA12)        │                  │              │              │
               GND ────────────────┴──────────────┴──────────────┘
```

| Part | Address | Supplies | Notes |
|---|---|---|---|
| U1 SHT45 | `0x44` | air temp, humidity | Needs a **vented** path to outside air. Keep it away from U2 and from the MCU — both self-heat |
| U2 SCD41 | `0x62` | CO2 | **~205 mA peaks.** Its own 10 µF local bulk, short and wide to GND |

Two placement rules that are easy to get wrong:

1. **The SHT45 must not sit downwind of the SCD41 or over a copper pour that
   reaches the MCU.** It is the accurate sensor on the board (±0.1 °C); mounting
   it next to a part that dissipates 205 mA in bursts throws that away. Give it a
   board cut-out or slots if you can, and put it at the edge.
2. **The SCD41 needs the same air the plants do.** Both parts want the enclosure
   vented — a sealed IP65 box measures the CO2 of its own interior, which is a
   number that means nothing. Use a **vented gland or a Gore-type membrane vent**,
   which keeps the IP rating while letting gas equilibrate.

The rail gate, upstream of **all** the channels:

```
   VBAT ──────┬──────────────── S │ Q1 (P-MOSFET) │ D ──┬──── VSENS
              │                     G                   │
            [100k]                  │                 [C1 bulk]
              │                     │                   │
              └─────────────────────┤                  GND
                                    │
   PB2 (SENS_GATE) ──[100R..1k]─────┘
```

`SENS_GATE` LOW pulls the gate below the source → P-FET conducts → `VSENS` live.
Hi-Z or HIGH → the 100 k holds gate at source → off.

| Part | Value | Why |
|---|---|---|
| Pull-up | **2.2 kΩ** | For the 10–20 m run. 4.7 kΩ is the short-bus value; 2.2 kΩ buys back rise time against cable capacitance. Keep ≥1.5 kΩ so the DS18B20 can still pull a valid low |
| Series R | **100 Ω** | Reflection damping, and the sacrificial element ahead of the TVS |
| TVS | bidirectional, **<50 pF** | 20 m of wet cable is an antenna. Capacitance is the spec that matters — high-C protection rounds off the bit slots |
| Cap at probe | **100 nF** | At the **far** end across the probe's VDD/GND, not on the PCB |
| Bulk on `VSENS` | **1–10 µF** | Bounded by the settle time below |
| I2C pull-ups | **4.7 kΩ** | On `VSENS`, like the 1-Wire ones. 100 kHz over a few cm of board wants nothing faster |
| SCD41 local bulk | **10 µF** | Handles its 205 mA current steps close to the part, so they do not appear on `VSENS` |

### The settle-time constraint

There are now **two** settle windows, and they pull in opposite directions:

| Constant | Value | What must be true by then |
|---|---|---|
| `DS_POWER_SETTLE_MS` | **10 ms** | `VSENS` fully up, so the first 1-Wire reset sees a valid bus |
| `I2C_POWER_SETTLE_MS` | **1000 ms** | The SCD41 will accept a command (its own datasheet power-up time) |

The firmware overlaps them — it starts the DS18B20 conversions at 10 ms and
spends the SCD41's remaining ~990 ms covering the 750 ms conversion, so the rail
is on for the **longer** of the two, not the sum. The board only has to satisfy
the 10 ms figure.

Keep total `VSENS` bulk at **≤10 µF plus the SCD41's local 10 µF** and put
**100 Ω–1 kΩ in series with the gate** to tame inrush — that lands around a
millisecond. If you raise the bulk further to quiet the SCD41's current steps,
raise `DS_POWER_SETTLE_MS` to match; the two are one design decision.

> Oversize that cap without touching the constant and the failure mode is
> intermittent CRC errors on the first read after each wake. Miserable to
> diagnose in the field.

### On the P-FET

The CO2 sensor changed what this part has to do. It used to switch ~3 mA; it now
switches **~205 mA peaks**, so check three things on your specific datasheet:

1. **Continuous drain current and R_DS(on)** at 3.3 V of gate drive. An
   AO3401-class part is still comfortable — at ~50 mΩ, 205 mA costs about 10 mV of
   drop — but it is no longer so overkill that it goes without checking.
2. **Drain-source leakage (I_DSS)**, still the parameter that matters most: at a
   15-minute duty cycle the part is off 99.9 % of the time. At ~1 µA it is the same
   order as the STM32's Stop2 current, i.e. the power gate becomes a real fraction
   of the battery budget. A low-leakage P-FET, or a load-switch IC with a spec'd
   sub-100 nA off current, is the upgrade.
3. **Inrush into the larger bulk.** More capacitance on `VSENS` means a bigger
   turn-on surge through the same FET; the gate series resistor is what limits it.

Route `VBAT` → Q1 → `VSENS` as a **wide** trace now, and give the SCD41 a short,
fat GND return. 205 mA down a thin trace is a visible droop on the rail that the
SHT45 and the DS18B20 both share.

---

## 4. Bill of materials — front-end PCB

| Ref | Part | Value / spec | Notes |
|---|---|---|---|
| J4–J7 | ARDUINO Uno V3 pin headers | **male**, underside: 8 / 6 / 8 / 10 pos, 2.54 mm | Mate CN6 / CN8 / CN9 / CN5. Note the **0.16″** D7↔D8 offset — use a proper Uno V3 footprint, do not lay it out on a 0.1″ grid |
| Q1 | P-MOSFET SOT-23 | A0341 / AO3401-class, Vgs(th) ≤ −1.5 V, ≥0.5 A | High-side gate; check I_DSS **and** that it carries the SCD41's 205 mA peaks |
| R1 | Resistor 0805 | 100 kΩ | Gate→source pull-up — **holds the rail off at reset** |
| R2 | Resistor 0805 | 100 Ω–1 kΩ | Gate series, inrush limit |
| R3, R4 | Resistor 0805 | 2.2 kΩ | DQ pull-ups, **on `VSENS`** |
| R5, R6 | Resistor 0805 | 100 Ω | DQ series |
| C1 | Ceramic | 1–10 µF | `VSENS` bulk (see settle time) |
| C2 | Ceramic | 100 nF | `VSENS` decoupling |
| D1, D2 | TVS bidirectional | <50 pF, ~5 V standoff | One per DQ line |
| U1 | **SHT45** | I2C `0x44`, DFN | Air temp + humidity. **Vented, at the board edge, away from U2** |
| U2 | **SCD41** | I2C `0x62` | CO2. Needs vented air and its own local bulk |
| R7, R8 | Resistor 0805 | 4.7 kΩ | I2C SDA/SCL pull-ups, **on `VSENS`** |
| C3 | Ceramic | 10 µF | SCD41 local bulk, close to the part |
| C4, C5 | Ceramic | 100 nF | SHT45 / SCD41 decoupling |
| J1, J2 | Pluggable screw terminal, 3-pin | Phoenix MC 1,5/3-ST-3,5 or clone | One per probe |
| J3 | Battery connector | keyed, 2-pin | LiFePO4 or 2× lithium AA |
| — | M3 nylon standoffs + screws | ×4 | Nucleo mounting holes |
| TP1–7 | Test pads | — | `VSENS`, both DQ, `SENS_GATE`, `VBAT`, SDA, SCL |
| — | Antenna keep-out | board cut-out | Over the RF section + **CN12 SMA**. Verify against a physical board (§1) |

At the probe end (not on the PCB): **100 nF** across each probe's VDD/GND.

---

## 5. Power — three traps

**The ST-LINK will eat the battery.** A Nucleo's debug section draws milliamps,
fatal for cells. Either feed 3V3 directly to the target at **CN6 pin 4** —
UM2592 Table 9 names `CN6 pin 4` and `CN7 pin 16` as the same 3V3 input and rates
it at **3 V to 3.6 V, 1.3 A**, and states plainly that "the programming and
debugging features are not available, since the ST-LINK is not powered" — or
**snap off the ST-LINK section**; these boards are scored for it.

> The shield reaches `CN6-4`, so the battery enters through the standard ARDUINO
> power header. There is no `VBAT` pin anywhere on these four connectors to
> confuse it with — the morpho's misleadingly-named `VBAT` (CN7-33, the RTC
> backup pin, *not* a power input) is out of reach. One less trap. Then measure: if Stop2 current is not
single-digit µA, something on the board is still alive.

**Battery chemistry is constrained by the DS18B20, not the MCU.** The STM32WL runs
to 1.8 V but the **DS18B20 needs ≥3.0 V**, so 2×AA alkaline (sagging to ~2.0 V)
kills the sensors long before the radio quits. Use **LiFePO4 (3.0–3.6 V)** or
**2× lithium AA (L91)** — the rail `battery.cpp` already assumes.

**The CO2 sensor now sets the peak-current requirement, and it is not close.**
The SCD41 pulls **~205 mA** during a measurement, against the LoRa TX's ~150 mA
peak and the DS18B20's few mA. Two consequences:

- **Cell internal resistance matters.** A cell that sags below 3.0 V under a
  200 mA pulse browns out the DS18B20 rail — and because the CO2 measurement
  happens *after* the water temperatures are read, the symptom is a node that
  looks fine for months and then starts resetting as the cells age. Check the
  cell's pulse-load spec, not just its capacity.
- **The measurement is not free.** A single shot blocks 5 s, and an honest
  reading needs two (the first after power-up is unsettled), so CO2 is paced by
  `CO2_EVERY_N_WAKES` in `node_config.h` — one reading per hour by default rather
  than one per wake. See README, "CO2 on a battery node", for the arithmetic.

> **Coupling to firmware:** `battery_read_mv()` reads VDDA via VREFINT. Add a
> regulator between battery and MCU and it starts reporting the regulator output
> instead of the battery — you would then need a divider plus an ADC pin added to
> the §2 contract.

---

## 6. Probe cable and enclosure

- **Probes:** 3-pin pluggable screw terminals. Field re-termination with cold hands
  is the design case; JST crimps will not survive it. Silkscreen `V / DQ / G` and
  mark **HOT** vs **COLD** — the distinction is purely which connector the probe
  lands in.
- **Cable:** one twisted pair = **DQ + GND**, VDD on a separate conductor. Cat5 is
  ideal. Biggest single reliability factor at 20 m.
- **Enclosure:** IP65, cable glands sized to the probe cable. Size the box for the
  **stacked** height, not the Nucleo alone.
- **Venting — new, and it is not optional.** The SHT45 and SCD41 measure the air
  they sit in. A sealed box measures the humidity and CO2 of its own interior,
  which is a number that means nothing and drifts with sunlight. Fit a **Gore-type
  membrane vent** (keeps IP65) or a vented gland, and mount both parts near it.
  Keep the SHT45 away from the SCD41 and from any copper that reaches the MCU —
  both self-heat, and ±0.1 °C of sensor accuracy is trivially destroyed by a few
  milliwatts of neighbouring dissipation.
- **RF:** keep the antenna clear of metal — see the keep-out in §1.
- **Test points:** the seven in the BOM. Seven pads turn a field failure into a
  30-second measurement.

---

## 7. Bring-up order

Do this before the boards go in a sealed box on a pole. The
[`bluepill_f103c8_dump`](../README.md) diagnostic is the right first power-on test
and ports to the WL55 in a few lines.

**Front-end alone, no Nucleo fitted** — this is why the test pads exist:

1. **Buzz out all eight signals** at the header strips against §2's pin map —
   `CN6-4`, `CN6-6`, `CN6-7`, `CN8-2`, `CN8-3`, `CN5-2`, `CN5-7`, `CN5-9`,
   `CN5-10`. Ten minutes with a multimeter here is the whole defence against a
   mirrored footprint, and it is the last moment the mistake is cheap. Confirm at
   the same time that **nothing** rings out to `CN9-7/8` (VCP) or `CN5-8` (AVDD).
2. Check the outline against a real board — the Uno V3 offset means a footprint
   laid out on a plain 0.1″ grid will not seat.
3. Bench supply on `VBAT`. Pull `SENS_GATE` high → `VSENS` = 0 V. Pull it low →
   `VSENS` = `VBAT`, and **all four** signal lines (both DQ, SDA, SCL) idle high
   through their pull-ups.

**Stacked:**

4. Gate off → sleep current is µA. With the rail off, SDA and SCL must read
   **0 V**, not a diode drop below `VBAT` — anything else means an I2C pull-up
   landed on permanent 3V3 instead of `VSENS`.
5. Pin probe → `pull-up=1 pull-down=1` on both DQ lines (proves the pull-up is
   really on `VSENS`, not GND).
6. Read probes → plausible independent temperatures, CRC OK.
7. **I2C scan** with the gate on → exactly `0x44` (SHT45) and `0x62` (SCD41)
   answer. Do this before trying to read either part: an address that does not
   appear is a wiring or pull-up fault, while an address that appears but returns
   CRC failures is a signal-integrity one, and the two want different fixes.
8. Read the SHT45 → plausible room temperature and humidity, and within ~1 °C of
   the DS18B20 probes if they are sitting in the same air.
9. Read the SCD41 → wait out its 1 s power-up, then a single shot. Outdoor air is
   **~420 ppm** and a room with a person in it runs 600–1200 ppm; a reading pinned
   near 400 on the first shot is exactly the unsettled-first-measurement artifact
   `CO2_SINGLE_SHOT_WARMUP` exists to discard.
10. Full cycle → wake, read, TX, sleep; confirm Stop2 current after the gate
   closes, and confirm the rail actually goes off **during** the sleep rather than
   at the next wake.

Steps 3 and 5 catch the resistor-to-GND class of fault — all-zero scratchpads that
pass CRC and decode as a convincing `0.00 °C`, which `ds18b20_read()` rejects
explicitly for this reason.

---

## References

- **UM2592** — STM32WL Nucleo-64 board (MB1389) user manual: the authority for
  this board. **Table 17** ("ARDUINO connectors pinout") is the source for §2's
  pin map and **Table 18** ("Pin assignment of the ST morpho connectors") for
  Appendix A; **Table 9** ("External power sources: 3V3") is the source for the
  CN6-4 battery input; §6.6.5 and the solder-bridge tables cover the VCP/D0-D1
  arrangement.
- Maxim/ADI **AN148** — guidelines for reliable long 1-Wire networks.
- DS18B20 datasheet — **VDD 3.0–5.5 V** is the constraint driving battery choice.
- Sensirion **SHT4x** datasheet — command set, timing, and the transfer functions
  ported in [`src/sht45.cpp`](../src/sht45.cpp).
- Sensirion **SCD4x** datasheet — command set, the 1 s power-up time, the 5 s
  single-shot duration, and the peak-current figure that sizes Q1. Ported in
  [`src/scd41.cpp`](../src/scd41.cpp).

---

## Appendix A — ST morpho pin map (not used by this design)

The front-end is a shield (§1), so **none of this is part of the interface** — the
morpho headers are left free. It is kept because those headers stay exposed on the
assembled node and are the natural place to hook a scope or a bench supply, and
because the table is tedious to re-derive.

Transcribed from **UM2592 Rev 1, Table 18** and script-verified against the PDF.
Both connectors are 2×19, 2.54 mm; odd pins are one row, even pins the other.

**CN7**

| Odd | Name | | Even | Name | |
|---:|---|---|---:|---|---|
| 1 | NC | | 2 | NC | |
| 3 | NC | | 4 | NC | |
| 5 | VDD_MCU | | 6 | E5V | |
| 7 | BOOT0 | | 8 | GND |  |
| 9 | NC | | 10 | NC | |
| 11 | NC | | 12 | IOREF | |
| 13 | PA13 | ⛔ SWD | 14 | NRST | |
| 15 | PA14 | ⛔ SWD | 16 | 3V3 |  |
| 17 | PA15 | | 18 | 5V | |
| 19 | GND |  | 20 | GND |  |
| 21 | NC | | 22 | GND | |
| 23 | PC13 | | 24 | VIN | |
| 25 | PC14 | | 26 | NC | |
| 27 | PC15 | | 28 | PB1 | |
| 29 | NC | | 30 | PB2 | |
| 31 | NC | | 32 | PA10 |  |
| 33 | VBAT |  | 34 | PB4 | |
| 35 | NC | | 36 | PB14 | |
| 37 | NC | | 38 | PB13 | |

**CN10**

| Odd | Name | | Even | Name | |
|---:|---|---|---:|---|---|
| 1 | PA0 | | 2 | PC4 | ⛔ RF switch FE_CTRL1 |
| 3 | PA12 |  | 4 | PC5 | ⛔ RF switch FE_CTRL2 |
| 5 | PA11 |  | 6 | NC | |
| 7 | AVDD | | 8 | 5V_USB_CHGR | |
| 9 | GND |  | 10 | NC | |
| 11 | PA5 | | 12 | PC6 | |
| 13 | PA6 | | 14 | PC0 | |
| 15 | PA7 | | 16 | PA8 |  |
| 17 | PA4 | | 18 | NC | |
| 19 | PA9 |  | 20 | GND |  |
| 21 | PC2 | | 22 | PB0 | ⛔ VDD_TCXO |
| 23 | PC1 | | 24 | NC | |
| 25 | PB10 | | 26 | PB9 | |
| 27 | PB8 | | 28 | PB15 | |
| 29 | PB5 | | 30 | PB11 | |
| 31 | PB3 | | 32 | AGND | |
| 33 | PB12 | | 34 | NC | |
| 35 | PB6 / PA2 | ⛔ VCP TX (D1) | 36 | PA1 | |
| 37 | PB7 / PA3 | ⛔ VCP RX (D0) | 38 | PC3 | ⛔ RF switch FE_CTRL3 |

> **An erratum in the source.** UM2592 Rev 1 prints CN10 pin 37 as "PB6 / PA3"; it
> is **PB7 / PA3**. PB6 is already at pin 35, PB7 appears nowhere else in a table
> whose own preamble says *all* MCU I/Os are on the morpho, and the note above
> that table states D0/D1 are USART1 on **PB6 and PB7**. ST's own STM32duino
> variant file agrees (`D0 = PB7`, and `PA3` "could be on D0"). The table above
> carries the corrected value.

Two morpho positions are worth knowing even on a shield build:

- **CN7-16 `3V3`** — the same battery input as `CN6-4`, if you would rather feed
  power from this side.
- **CN7-33 `VBAT`** — the STM32's **RTC backup** pin, *not* a power input. It
  cannot run the MCU or the radio. The name is the trap; do not wire a battery to
  it.
- **CN7-13/15 `PA13`/`PA14`** — SWD, the only place to get debug out, since the
  ARDUINO headers do not carry it.
