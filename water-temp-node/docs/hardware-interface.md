# water-temp-node — hardware interface spec

The real (non-breadboard) build is **two PCBs joined by a connector**:

| Board | What it is | Carries |
|---|---|---|
| **Brain** | NUCLEO-WL55JC1 (off-the-shelf) | STM32WL55JC, radio, RF switch, TCXO, antenna, ST-LINK |
| **Front-end** | Custom PCB, this spec | DS18B20 probe connectors, SHT45 + SCD41 (I2C), A0341 P-MOSFET rail gate, pull-ups, line protection, battery input |

The front-end mounts **above** the Nucleo on its ST morpho headers. This document
specifies that joint: the connector, the signals crossing it, and the front-end
circuit behind them.

Everything here is a contract the **firmware already assumes**. Where a value is
forced by code, the code is cited — change one and you change the other.

- Firmware pin map: [`include/node_config.h`](../include/node_config.h)
- Gate + sleep handling: [`src/main.cpp`](../src/main.cpp)
- Cable-length guidance: [README → *Long DS18B20 runs*](../README.md)

---

## 1. The stack

```
        ┌─────────────────────────────────────────────┐
        │  FRONT-END PCB                              │
        │  probe J1/J2 · gate Q1 · pull-ups · TVS     │   <- this spec
        │  SHT45 U1 · SCD41 U2 (I2C) · battery J3     │
        │   [2x19 socket]            [2x19 socket]    │
        │    CN7-16 = BATT            CN10-3/5 = I2C  │
        │    CN7-32 = DQ_HOT          CN10-16 = GATE  │
        │                             CN10-19 = DQ_COLD│
        └──────┬──────────────────────────┬───────────┘
               │  CN7                CN10 │              2.54 mm, 38 pos each
        ┌──────┴──────────────────────────┴───────────┐
        │  NUCLEO-WL55JC1     [antenna keep-out]      │
        │  (ST-LINK section snapped off — see §5)     │
        └─────────────────────────────────────────────┘
```

- **Mating:** the Nucleo's morpho **CN7/CN10** are 2×19 male pin headers on
  2.54 mm pitch. The front-end carries two matching **2×19 female socket strips**
  on its underside.
- **Mechanical:** sockets at both long edges support the board at both edges — no
  standoff strictly required, but fit **M3 nylon standoffs** at the Nucleo's
  mounting holes anyway. A board held only by header friction will fret its
  contacts loose under vibration and thermal cycling.
- **Stack height:** the socket strip must clear the Nucleo's tallest parts (USB
  connector, antenna connector). Use tall/stacking sockets or add spacers, and
  confirm against your actual board before ordering.
- **Antenna keep-out:** cut the front-end board away over the Nucleo's RF section
  and antenna. No copper, no battery, no probe cable routed over it.

### Keying — do this, it is the one irreversible mistake

Two identical 2×19 headers, symmetric about the board centre, will mate **rotated
180°** just as happily as the right way round. That swaps CN7 with CN10 and
reverses the pin order, so the battery leaves `CN7-16` and arrives somewhere in
CN10's I/O — see §2's pin map for what lives there. Battery voltage onto a GPIO
kills the pin, and possibly the RF switch control next to it.

Pick at least one:

1. **Depopulate one position as a key** — clip a single unused pin on the Nucleo
   header and plug the matching socket hole. Cheap and absolute.
2. **Asymmetric outline** — extend the front-end board over one end only, so it
   fouls the USB/antenna if reversed.
3. **Silkscreen pin-1 triangles** on both boards, plus a printed arrow. Necessary
   but *not* sufficient on its own — never rely on this alone.

---

## 2. Signals crossing the connector

**Eight** signals. Freeze this table; it is the interface. Everything else on the
morpho headers is **left unconnected** on the front-end — do not casually route
extra pins "in case", each one is a new way to fight the radio or the debugger.

| Signal | WL55 pin | Morpho | Direction | Electrical | Forced by |
|---|---|---|---|---|---|
| `VBAT` | — | **CN7-16** (`3V3`) | in to brain | 3.0–3.6 V, ≤250 mA peak | `battery.cpp` — battery **is** VDDA |
| `GND` | — | **CN7-20**, **CN10-9** | — | use **≥2 GND pins**, one per header | — |
| `SENS_GATE` | **PA8** | **CN10-16** | out of brain, push-pull | active **LOW** = rail on; Hi-Z = off via 100 k | `node_config.h` `DS_PWR_*` |
| `DQ_HOT` | **PA10** | **CN7-32** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_HOT_*` |
| `DQ_COLD` | **PA9** | **CN10-19** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_COLD_*` |
| `I2C_SDA` | **PA11** | **CN10-5** | bidir, open-drain | I2C2 @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SDA_PIN` |
| `I2C_SCL` | **PA12** | **CN10-3** | out of brain, open-drain | I2C2 @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SCL_PIN` |
| `VSENS` | — | — | front-end internal | gated 3V3 to probes, I2C parts **and** all pull-ups | `main.cpp` `gate_on()` |

The peak figure is the **SCD41's ~205 mA measurement**, not the LoRa TX's ~150 mA:
the firmware gates the sensor rail **off** before it powers the radio, so the two
never overlap. Size `VBAT` for the larger one, not their sum.

`VSENS` is generated **on the front-end** and never crosses back to the Nucleo.
The only power crossing the joint is `VBAT` and ground.

Optional, bring-up only, not part of the contract: SWD (PA13/PA14) and the
LPUART1 VCP (PA2/PA3).

> **This table grew from six signals to eight** when the SHT45 and SCD41 were
> added. If you already have front-end boards fabricated to the six-signal
> revision, they are not forward-compatible — PA11/PA12 are not routed on them.

### Two rules the front-end must honor

**1. The pull-ups sit on `VSENS`, never on permanent 3V3.** This now covers
**four** pull-ups — two 1-Wire and two I2C. On the always-on rail, each gated-off
part becomes a leakage path through its clamp diodes and the 15-minute duty cycle
stops meaning anything. `sensor_pins_park()` in `main.cpp` parks all four pins as
analog for the same reason — the board has to cooperate.

**2. `SENS_GATE` is active-low with a 100 k gate→source pull-up.** That resistor is
not optional: it holds the sensor rail **off** while PA8 is Hi-Z — during reset,
during BOOT0, and before firmware runs.

### Pin choice

PA8/PA9/PA10/PA11/PA12 deliberately avoid PC3/PC4/PC5 (RF switch), PA13/PA14
(SWD), PA2/PA3 (VCP) and PB0 (TCXO) — see the header comment at the top of
`node_config.h`.

The I2C pair is **I2C2 on PA11/PA12**, not the more obvious alternatives:
**I2C1** would want PA9/PA10, which are the DS18B20 probes; **I2C3** would want
PB10/PB11, and **PB11 is the Nucleo's LED3** — SDA would drive an LED, loading
the bus and wasting current on every transfer. Keeping the whole sensor interface
inside port A also groups it on the morpho headers.

### The morpho pin map

Transcribed from **UM2592 Rev 1, Table 18** ("Pin assignment of the ST morpho
connectors"), the authority for this board. Both connectors are **2×19, 2.54 mm**;
odd pins are one row, even pins the other. Signals this node uses are **bold** and
marked ✅; ⛔ marks positions the front-end must never touch (§"Do not route
these"). Everything unmarked is simply left open.

**CN7**

| Odd | Name | Use | Even | Name | Use |
|---:|---|---|---:|---|---|
| 1 | NC | | 2 | NC | |
| 3 | NC | | 4 | NC | |
| 5 | VDD_MCU | | 6 | E5V | |
| 7 | BOOT0 | | 8 | **GND** | ✅ use |
| 9 | NC | | 10 | NC | |
| 11 | NC | | 12 | IOREF | |
| 13 | PA13 | ⛔ SWD | 14 | NRST | |
| 15 | PA14 | ⛔ SWD | 16 | **3V3** | ✅ **battery in** |
| 17 | PA15 | | 18 | 5V | |
| 19 | **GND** | ✅ use | 20 | **GND** | ✅ use |
| 21 | NC | | 22 | GND | |
| 23 | PC13 | | 24 | VIN | |
| 25 | PC14 | | 26 | NC | |
| 27 | PC15 | | 28 | PB1 | |
| 29 | NC | | 30 | PB2 | |
| 31 | NC | | 32 | **PA10** | ✅ `DQ_HOT` |
| 33 | VBAT | ⚠️ **not** battery in — see below | 34 | PB4 | |
| 35 | NC | | 36 | PB14 | |
| 37 | NC | | 38 | PB13 | |

**CN10**

| Odd | Name | Use | Even | Name | Use |
|---:|---|---|---:|---|---|
| 1 | PA0 | | 2 | PC4 | ⛔ RF switch FE_CTRL1 |
| 3 | **PA12** | ✅ `I2C_SCL` | 4 | PC5 | ⛔ RF switch FE_CTRL2 |
| 5 | **PA11** | ✅ `I2C_SDA` | 6 | NC | |
| 7 | AVDD | | 8 | 5V_USB_CHGR | |
| 9 | **GND** | ✅ use | 10 | NC | |
| 11 | PA5 | | 12 | PC6 | |
| 13 | PA6 | | 14 | PC0 | |
| 15 | PA7 | | 16 | **PA8** | ✅ `SENS_GATE` |
| 17 | PA4 | | 18 | NC | |
| 19 | **PA9** | ✅ `DQ_COLD` | 20 | **GND** | ✅ use |
| 21 | PC2 | | 22 | PB0 | ⛔ VDD_TCXO |
| 23 | PC1 | | 24 | NC | |
| 25 | PB10 | | 26 | PB9 | |
| 27 | PB8 | | 28 | PB15 | |
| 29 | PB5 | | 30 | PB11 | |
| 31 | PB3 | | 32 | AGND | |
| 33 | PB12 | | 34 | NC | |
| 35 | PB6 / PA2 | ⛔ VCP TX (D1) | 36 | PA1 | |
| 37 | PB7 / PA3 | ⛔ VCP RX (D0) | 38 | PC3 | ⛔ RF switch FE_CTRL3 |

> **Two errata in the source, both worth knowing.** UM2592 Rev 1 prints CN10 pin
> 37 as "PB6 / PA3"; it is **PB7 / PA3**. PB6 is already at pin 35, PB7 appears
> nowhere else in a table whose own preamble says *all* MCU I/Os are on the
> morpho, and the note above that table states D0/D1 are USART1 on **PB6 and
> PB7**. ST's own STM32duino variant file agrees (`D0 = PB7`, and `PA3` "could be
> on D0"). Neither pin is ours, but do not lose an afternoon to it.

So the eight contract signals land like this:

| Signal | MCU pin | Connector | Pin |
|---|---|---|---|
| `VBAT` (battery in) | — | **CN7** | **16** (`3V3`) |
| `GND` | — | **CN7** | **20** |
| `GND` | — | **CN10** | **9** |
| `SENS_GATE` | PA8 | **CN10** | **16** |
| `DQ_HOT` | PA10 | **CN7** | **32** |
| `DQ_COLD` | PA9 | **CN10** | **19** |
| `I2C_SDA` | PA11 | **CN10** | **5** |
| `I2C_SCL` | PA12 | **CN10** | **3** |
| `VSENS` | — | — | front-end internal, does not cross |

**Seven of the eight are on CN10; CN7 carries only `DQ_HOT`, power and ground.**
That asymmetry is worth knowing when you place parts — the probe connectors want
to be near the CN7 edge, everything else near CN10.

### `VBAT` means two different things — do not mix them up

This is the single most expensive mistake available on this connector, because
both candidates are plausibly named.

- **CN7 pin 16, `3V3`** — the board's **3.3 V power input**. This is where the
  battery goes. UM2592 Table 9: *3 V to 3.6 V, 1.3 A max*, which brackets our
  3.0–3.6 V chemistry exactly.
- **CN7 pin 33, `VBAT`** — the STM32WL's **backup-domain** pin, for keeping the
  RTC alive from a coin cell while the main rail is down. It is not a power input
  for the board, it cannot run the MCU, and it will not run the radio.

Our §2 signal is called `VBAT` because it is *the battery*; the morpho pin called
`VBAT` is something else entirely. **Silkscreen the front-end `BATT → CN7-16`**,
not `VBAT`, so the board itself disambiguates.

Powering through CN7 pin 16 leaves the ST-LINK unpowered, so **debugging stops
working** — the same trade §5 describes. That is expected, not a fault.

### Do not route these

Four positions on CN10 and two on CN7 are actively dangerous to touch, and a
front-end board that is 90 % copper pour makes it easy to touch them by accident:

| Position | Signal | Why |
|---|---|---|
| CN10 2, 4, 38 | PC4, PC5, PC3 | **RF switch** FE_CTRL1/2/3. Load these and the radio's TX/RX path misbehaves in ways that look like a bad antenna |
| CN10 22 | PB0 | **VDD_TCXO**. The radio's reference oscillator supply |
| CN7 13, 15 | PA13, PA14 | **SWD**. UM2592 explicitly advises against using them as I/O |
| CN10 35, 37 | PB6/PA2, PB7/PA3 | The **ST-LINK VCP**, i.e. the debug log |

Everything else not in the eight-signal table is simply left open.

### Orientation — the sockets are on the underside

The front-end's two socket strips face **down**. Everything above is written as
the Nucleo's own pin numbering, which is what you read off the board with it
component-side up. On the front-end's layout those same positions **mirror**:

```
   Nucleo, viewed from above          Front-end, viewed from above
   (male headers, component side)     (female sockets, on the underside)

   CN7            CN10                CN10                    CN7
   [1 ][2 ]      [1 ][2 ]             [2 ][1 ]               [2 ][1 ]
   [3 ][4 ]      [3 ][4 ]             [4 ][3 ]               [4 ][3 ]
    ...            ...                  ...                    ...
   [37][38]      [37][38]             [38][37]               [38][37]
```

Get this wrong and every signal is off by a column — which, because the odd row
is largely I/O and the even row carries `3V3`/`GND`/`NRST`, means battery voltage
onto GPIO. Lay the footprint out from the **mating** view and have someone else
check it against the tables above.

This is also why §1 insists on a physical key. The pin map does not save you from
a board fitted 180° rotated; only a blocked position does.

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
   PA10 ──[100R]────┼──────────────────────────────────┼────────── DQ
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
   PA11 ┼───────┼──── SDA ─────────┤ U1 SHT45     ┤ U2 SCD41     │
   (SDA)│       │                  │   (0x44)     │   (0x62)     │
   PA12 ────────┼──── SCL ─────────┤              ┤              │
   (SCL)        │                  │              │              │
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
   PA8 (SENS_GATE) ──[100R..1k]─────┘
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
| J4, J5 | Socket strip 2×19, 2.54 mm | female, underside | Mates CN7/CN10. **Key one position** (§1) |
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

At the probe end (not on the PCB): **100 nF** across each probe's VDD/GND.

---

## 5. Power — three traps

**The ST-LINK will eat the battery.** A Nucleo's debug section draws milliamps,
fatal for cells. Either feed 3V3 directly to the target at **CN7 pin 16** —
UM2592 Table 9 rates that input at **3 V to 3.6 V, 1.3 A**, and states plainly
that "the programming and debugging features are not available, since the ST-LINK
is not powered" — or **snap off the ST-LINK section**; these boards are scored for
it. Then measure: if Stop2 current is not
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

1. **Buzz out all eight signals** at the socket strips against §2's pin map —
   `CN7-16`, `CN7-20`, `CN7-32`, `CN10-3`, `CN10-5`, `CN10-9`, `CN10-16`,
   `CN10-19`. Ten minutes with a multimeter here is the whole defence against a
   mirrored footprint, and it is the last moment the mistake is cheap. Confirm at
   the same time that **nothing** rings out to `CN10-2/4/22/38` or `CN7-13/15`
   (the RF-switch, TCXO and SWD pins from "Do not route these").
2. Confirm the keyed position is blocked.
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
  this board. **Table 18** ("Pin assignment of the ST morpho connectors") is the
  source for §2's pin map; **Table 9** ("External power sources: 3V3") is the
  source for the CN7-16 battery input; §6.6.5 and the solder-bridge tables cover
  the VCP/D0-D1 arrangement.
- Maxim/ADI **AN148** — guidelines for reliable long 1-Wire networks.
- DS18B20 datasheet — **VDD 3.0–5.5 V** is the constraint driving battery choice.
- Sensirion **SHT4x** datasheet — command set, timing, and the transfer functions
  ported in [`src/sht45.cpp`](../src/sht45.cpp).
- Sensirion **SCD4x** datasheet — command set, the 1 s power-up time, the 5 s
  single-shot duration, and the peak-current figure that sizes Q1. Ported in
  [`src/scd41.cpp`](../src/scd41.cpp).
