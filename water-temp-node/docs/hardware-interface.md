# water-temp-node — hardware interface spec

The real (non-breadboard) build is **two PCBs joined by a cable**:

| Board | What it is | Carries |
|---|---|---|
| **Brain** | NUCLEO-WL55JC1 (off-the-shelf) | STM32WL55JC, radio, RF switch, TCXO, antenna, ST-LINK |
| **Front-end** | Large custom PCB, this spec | **6× DS18B20 probe connectors**, **3× SHT45** + SCD41 (I2C, the SHT45s behind a bus switch), A0341 P-MOSFET rail gate, pull-ups, line protection, battery input |

The front-end is a **large board sitting beside the Nucleo**, not stacked on it,
and the two are joined by a short cable. This document specifies that joint: the
connectors, the signals crossing them, and the front-end circuit behind them.

Everything here is a contract the **firmware already assumes**. Where a value is
forced by code, the code is cited — change one and you change the other.

- Firmware pin map: [`include/node_config.h`](../include/node_config.h)
- Gate + sleep handling: [`src/main.cpp`](../src/main.cpp)
- Wire frame (probe → metric): [`src/lora/lora_packet.h`](../src/lora/lora_packet.h)
- Cable-length guidance: [README → *Long DS18B20 runs*](../README.md)

---

## 1. The joint — which bus, and why

Six probe connectors, two gas/humidity sensors, protection and a battery input do
not fit on a 68.6 × 53.4 mm ARDUINO outline. Once the front-end has to be its own
large board it becomes a cabled peripheral rather than anything that mounts on
the Nucleo — which is what decides the connector question.

### The recommendation

**Two cables, deliberately separate:**

| Cable | Carries | Nucleo end | Front-end end |
|---|---|---|---|
| **Signal** | `SENS_GATE`, 6× `DQ_Pn`, `SDA`, `SCL`, 2× `GND` | **2×19 IDC socket over the whole of morpho CN10** | shrouded, keyed 2×19 box header |
| **Power** | `VBAT`, `GND` | **1×8 socket over the whole of ARDUINO CN6** (only pins 4/6/7 wired) | keyed, latching 2-pin (JST-XH or Micro-Fit 3.0) |

Signal cable: 2.54 mm, 40-way rainbow ribbon cut to **38 conductors**, IDC
mass-terminated at both ends. Keep it **≤ 30 cm**.

### Why this and not the alternatives

**Why the morpho header, not the ARDUINO headers.** The ARDUINO connectors are
four *single-row* strips (8/6/8/10) carrying only **24** of the MCU's I/Os. Cabling
to them means four separate cables, and it cannot reach PA8. Morpho **CN10** is
a single 2×19 that carries every logic signal this design needs, so one ribbon
does the whole job.

**Why one full-length socket instead of a small one.** A 2×19 IDC socket spans
the *entire* CN10 header, so it physically cannot be fitted offset by a position.
A 1×10 socket on an ARDUINO header, or a 2×5 on part of the morpho, can — and a
one-position slip moves every signal at once, which is the kind of fault that
survives a whole afternoon of debugging.

**Why power is a separate cable, and this is the important one.** A 2×19 socket on
an unshrouded pin field *can* be pressed on **rotated 180°**. If `VBAT` were on
that ribbon, one bad insertion puts battery voltage onto a GPIO. With power on its
own keyed 2-pin lead, the worst a reversed ribbon can do is short a couple of
open-drain GPIOs to ground and cross-connect signals — unpleasant, recoverable, not
a dead board. It also keeps the battery's 205 mA supply current off a bundle
carrying six bit-banged 1-Wire lines.

**Why not loose Dupont jumpers.** Twelve independent crimps with no latch and no
key. Each one is a field failure waiting for the first thermal cycle. If you
breadboard it that way for bring-up, do not ship it that way.

**Why not RS-485 / a differential link per probe.** Not needed at these lengths —
plain 1-Wire handles 10–20 m (Maxim **AN148**), and a MAX485 *cannot* sit on a
1-Wire line at all. See README, "Long DS18B20 runs". Past ~30 m, escalate to a
DS2483 line driver, not to a different cable between these two boards.

### Keying the signal ribbon — do this, it is nearly free

**CN10-6 is NC on the Nucleo.** So:

1. **Clip (or desolder) the CN10-6 pin.** It connects to nothing — verify with a
   meter first, then remove it.
2. **Plug the matching hole in the IDC socket** with the key plug that ships with
   most IDC sockets, or a blob of epoxy.

Rotated 180°, that plugged hole lands on **position 33 (PB12)**, which *is*
populated — so the socket will not seat. A physical stop beats a silkscreen arrow.

Still worth doing on top:

- **Silkscreen a pin-1 triangle** on the front-end box header and mark the ribbon's
  red stripe = pin 1 at both ends.
- **Different-sized connectors** already stop the two cables being swapped: 2×19
  ribbon versus a 2-pin power lead.

```
   ┌────────────────────────────────────────────┐
   │  FRONT-END PCB (large, custom)             │
   │  J1..J6 probe terminals · Q1 gate · TVS    │   <- this spec
   │  SHT45 U1 · SCD41 U2 (I2C) · battery J8    │
   │                                            │
   │   [2x19 box header, keyed]      [2-pin]    │
   └────────────┬──────────────────────┬────────┘
                │ 38-way ribbon        │ VBAT + GND
                │ <= 30 cm             │ (keyed, latching)
   ┌────────────┴──────────────────────┴────────┐
   │  NUCLEO-WL55JC1                            │
   │  CN10 (2x19 morpho)              CN6 (1x8) │
   │  [antenna keep-out — keep the cable clear] │
   └────────────────────────────────────────────┘
```

**Mechanical:** the cable takes no mechanical load. Fit **M3 nylon standoffs** at
both boards' mounting holes and add a **strain relief / cable tie** within 30 mm of
each connector. An IDC socket held only by contact friction will fret loose under
vibration and thermal cycling long before anything electrical fails.

**Antenna keep-out:** route neither cable over the Nucleo's RF section or the
**CN12 SMA**. Easy here, since the front-end does not sit over the radio — but a
30 cm ribbon draped across the antenna will detune it, so route it deliberately.

---

## 2. Signals crossing the connector

**Twelve** signals. Freeze this table; it is the interface. Every other position
on CN10 is **left unconnected** at the front-end.

Note what is *not* here: the three SHT45s add **no** signals. They are separated
on the front-end by a bus switch that is itself an I2C device (§3), so the joint
stays at twelve. Resolving their shared address with three individual power
gates instead would have cost three more — and would not have worked; see below.

| Signal | WL55 pin | Connector | Direction | Electrical | Forced by |
|--------|----------|-----------|-----------|------------|-----------|
| `VBAT` | — | **CN6-4** (`3V3`) | in to brain | 3.0–3.6 V, ≤250 mA peak | `battery.cpp` — battery **is** VDDA |
| `GND` (power) | — | **CN6-6/7** | — | the battery return, its own conductor | — |
| `SENS_GATE` | **PA8** | **CN10-16** | out of brain, push-pull | active **LOW** = rail on; Hi-Z = off via 100 k | `node_config.h` `DS_PWR_*` |
| `DQ_P0` | **PA5** | **CN10-11** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_PROBE_BUSES` |
| `DQ_P1` | **PA4** | **CN10-17** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P2` | **PA9** | **CN10-19** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P3` | **PC2** | **CN10-21** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P4` | **PC1** | **CN10-23** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P5` | **PB10** | **CN10-25** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `I2C_SDA` | **PA11** | **CN10-5** | bidir, open-drain | I2C @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SDA_PIN` |
| `I2C_SCL` | **PA12** | **CN10-3** | out of brain, open-drain | I2C @ 100 kHz | `node_config.h` `I2C_SCL_PIN` |
| `GND` (signal) | — | **CN10-9**, **CN10-20** | — | **both**, one per end of the DQ block | — |
| `VSENS` | — | — | front-end internal | gated 3V3 to probes, I2C parts **and** all pull-ups | `main.cpp` `gate_on()` |

The 250 mA peak figure is the **SCD41's ~205 mA measurement**, not the LoRa TX's
~150 mA: the firmware gates the sensor rail **off** before it powers the radio, so
the two never overlap. Size `VBAT` for the larger one, not their sum. The six
probes add ~9 mA to that — noise, and they draw it during the conversion, before
the CO2 measurement starts.

`VSENS` is generated **on the front-end** and never crosses back to the Nucleo. The
only power crossing the joint is `VBAT` and its ground, on their own cable.

### Two rules the front-end must honor

**1. The pull-ups sit on `VSENS`, never on permanent 3V3.** That is **eight**
pull-ups — six 1-Wire and two I2C. On the always-on rail, each gated-off part
becomes a leakage path through its clamp diodes and the 15-minute duty cycle stops
meaning anything. `sensor_pins_park()` in `main.cpp` parks every one of those pins
as analog for the same reason — the board has to cooperate.

**2. `SENS_GATE` is active-low with a 100 k gate→source pull-up.** That resistor is
not optional: it holds the sensor rail **off** while PA8 is Hi-Z — during reset,
during BOOT0, and before firmware runs.

### Why these pins

**The probes are placed by ribbon adjacency, not by convenience.** Six bit-banged
open-drain lines sharing one flat cable is the one real signal-integrity problem
in this design. In a standard IDC ribbon, conductor *n* goes to pin *n*,
so a signal at position *p* is flanked in the cable by positions *p−1* and *p+1*.
CN10's ground pins are fixed by the board — you cannot add more — so the six DQ
lines were placed to sit next to a **GND** or an **NC** position wherever
possible.

```
   Conductor n of the ribbon lands on CN10 pin n, so a line's cable
   neighbours are simply pins n-1 and n+1:

   CN10 pin :    9   10   11   12   13   14   15   16   17   18   19   20   21   22   23   24   25   26
   carries  :  GND  [g]   P0    -    -    -    - GATE   P1  [g]   P2  GND   P3    -   P4  [g]   P5    -

   Every DQ line touches a GND (9, 20) or a guard [g] (10, 18, 24).
```

In full:

| Probe | Pin | CN10 | Lower neighbour | Upper neighbour |
|---|---|---:|---|---|
| `DQ_P0` | PA5 | 11 | **10 = NC** (guard) | 12 = PC6, unconnected |
| `DQ_P1` | PA4 | 17 | 16 = `SENS_GATE` (static DC) | **18 = NC** (guard) |
| `DQ_P2` | PA9 | 19 | **18 = NC** (guard) | **20 = GND** ✅ |
| `DQ_P3` | PC2 | 21 | **20 = GND** ✅ | 22 = PB0, unconnected |
| `DQ_P4` | PC1 | 23 | 22 = PB0, unconnected | **24 = NC** (guard) |
| `DQ_P5` | PB10 | 25 | **24 = NC** (guard) | 26 = PB9, unconnected |

`SENS_GATE` at CN10-16 is a deliberately benign neighbour for `DQ_P1`: it changes
state twice per 15-minute wake and is otherwise a static DC level.

**Guard conductors.** CN10 positions **6, 10, 18, 24 and 34 are NC** on the Nucleo,
so their ribbon conductors may be tied to **GND at the front-end end only**. That is
a one-ended guard — it reduces capacitive crosstalk but is *not* a return path.
The two real returns are **CN10-9** and **CN10-20**, and both must be used.

> Verify with a meter that those NC positions really are open on *your* board
> before grounding them. They are NC in UM2592 Table 18, but grounding a position
> that turned out to be a GPIO is a short.

**Every other conductor must float at the front-end.** Do not ground unused MCU
positions "to be tidy" — a firmware bug that drives one then shorts a push-pull
output. Do not connect the ⛔ positions below at all.

**`SENS_GATE` is PA8** (CN10-16). It sits inside the DQ block, so the gate and the
lines it powers travel the same ribbon; its alternate functions are unused here;
and the STM32WL has no boot strap on it (BOOT0 is PH3, nBOOT1 is an option byte),
so it resets to a floating input — exactly what the 100 k gate pull-up needs to
hold the rail off before firmware runs.

**PA10 is deliberately unused**, despite being a perfectly good 1-Wire pin. It is
on **CN7**, not CN10, and one line does not justify a second signal ribbon.

**I2C is on PA11/PA12** (CN10-5 / CN10-3). The alternatives are worse: I2C1 is
PA9/PA10 (PA9 is a probe) and I2C3 is PB10/PB11 (PB10 is a probe, and PB11 is the
Nucleo's **LED3**, which would sit on SDA and burn current every transfer).

> **One label to verify.** UM2592 Table 17 calls PA11/PA12 `I2C1_SDA`/`I2C1_SCL`,
> but ST's own STM32duino `PeripheralPins.c` and Zephyr's board definition both
> map them to **I2C2**. The firmware is unaffected either way — STM32duino
> resolves the instance from the pin map, not from a name — but if you need the
> instance for a bare-HAL port, check RM0453's alternate-function table rather
> than trusting either source.

> **SCL sits between the two RF-switch control lines** (CN10-2 `PC4` and CN10-4
> `PC5`), whose conductors are unterminated stubs at the front-end. This is
> tolerable only because of *temporal* separation: the firmware reads every sensor,
> gates the rail off, and only then powers the radio, so the I2C bus is never
> active while those lines switch. Do not reorder that sequence in `main.cpp`.
> If you ever need I2C concurrent with TX, slit conductors 1–4 out of the ribbon.

### The morpho CN10 pin layout

This is the picture to hold next to the board while wiring the front-end
connector — the same data as the table below, laid out the way the header
physically sits.

```
                  CN10 - 2x19 morpho, 2.54 mm
        Nucleo component side up. Odd pins are one row, even the other.
        Confirm pin 1 against the board's own silkscreen before wiring --
        this drawing fixes the ORDER, not which end of the board it starts.

        [+] front-end wires it      [K] key: clip pin, plug socket hole
        [g] guard: GND at the FRONT-END end only, never at the Nucleo
        [X] do NOT connect          [!] usable, but read the note
            (blank) leave the conductor floating at the front-end

   front-end                MCU     odd  even MCU     front-end
   ------------------------ ------- ---- ---- ------- ------------------------
       B1 button            PA0       1 o  o 2   PC4     [X] RF FE_CTRL1
   [+] I2C_SCL              PA12      3 o  o 4   PC5     [X] RF FE_CTRL2
   [+] I2C_SDA              PA11      5 o  o 6   NC      [K] KEY - clip this pin
   [X] VREF+ - do not load  AVDD      7 o  o 8   5V_USB_CHGR
   [+] signal return A      GND       9 o  o 10  NC      [g] guard
   [+] DQ_P0                PA5      11 o  o 12  PC6         B3 button
       spare                PA6      13 o  o 14  PC0         spare
       spare                PA7      15 o  o 16  PA8     [+] SENS_GATE
   [+] DQ_P1                PA4      17 o  o 18  NC      [g] guard
   [+] DQ_P2                PA9      19 o  o 20  GND     [+] signal return B
   [+] DQ_P3                PC2      21 o  o 22  PB0     [X] VDD_TCXO
   [+] DQ_P4                PC1      23 o  o 24  NC      [g] guard
   [+] DQ_P5                PB10     25 o  o 26  PB9         LED2
       spare                PB8      27 o  o 28  PB15        LED1
       spare                PB5      29 o  o 30  PB11        LED3
   [!] also TRACESWO        PB3      31 o  o 32  AGND    [X] not a signal return
       spare                PB12     33 o  o 34  NC      [g] guard
   [X] VCP TX               PB6/PA2  35 o  o 36  PA1         B2 button
   [X] VCP RX               PB7/PA3  37 o  o 38  PC3     [X] RF FE_CTRL3
```

### The morpho CN10 pin map

Transcribed from **UM2592 Rev 1, Table 18** and script-verified against the PDF.
Both morpho connectors are 2×19, 2.54 mm; odd pins are one row, even pins the
other. Signals this node uses are **bold** ✅; ⛔ marks positions the front-end
must not connect. The CN7 map is in
[Appendix A](#appendix-a--st-morpho-cn7-not-used-by-the-signal-cable).

| Odd | Name | Use | Even | Name | Use |
|---:|---|---|---:|---|---|
| 1 | PA0 | button B1 — leave open | 2 | PC4 | ⛔ RF switch FE_CTRL1 |
| 3 | **PA12** | ✅ `I2C_SCL` | 4 | PC5 | ⛔ RF switch FE_CTRL2 |
| 5 | **PA11** | ✅ `I2C_SDA` | 6 | NC | 🔑 **key: clip this pin** |
| 7 | AVDD | ⛔ VREF+ — do not load | 8 | 5V_USB_CHGR | leave open |
| 9 | **GND** | ✅ signal return A | 10 | NC | guard (GND at front-end only) |
| 11 | **PA5** | ✅ `DQ_P0` | 12 | PC6 | button B3 — leave open |
| 13 | PA6 | spare | 14 | PC0 | spare |
| 15 | PA7 | spare | 16 | **PA8** | ✅ `SENS_GATE` |
| 17 | **PA4** | ✅ `DQ_P1` | 18 | NC | guard (GND at front-end only) |
| 19 | **PA9** | ✅ `DQ_P2` | 20 | **GND** | ✅ signal return B |
| 21 | **PC2** | ✅ `DQ_P3` | 22 | PB0 | ⛔ VDD_TCXO |
| 23 | **PC1** | ✅ `DQ_P4` | 24 | NC | guard (GND at front-end only) |
| 25 | **PB10** | ✅ `DQ_P5` | 26 | PB9 | LED2 — leave open |
| 27 | PB8 | spare | 28 | PB15 | LED1 — leave open |
| 29 | PB5 | spare | 30 | PB11 | LED3 — leave open |
| 31 | PB3 | ⚠️ also TRACESWO | 32 | AGND | ⛔ analog ground, not a return |
| 33 | PB12 | spare (stops a rotated socket) | 34 | NC | guard (GND at front-end only) |
| 35 | PB6 / PA2 | ⛔ VCP TX (D1) | 36 | PA1 | button B2 — leave open |
| 37 | PB7 / PA3 | ⛔ VCP RX (D0) | 38 | PC3 | ⛔ RF switch FE_CTRL3 |

> **An erratum in the source.** UM2592 Rev 1 prints CN10 pin 37 as "PB6 / PA3"; it
> is **PB7 / PA3**. PB6 is already at pin 35, PB7 appears nowhere else in a table
> whose own preamble says *all* MCU I/Os are on the morpho, and the note above
> that table states D0/D1 are USART1 on **PB6 and PB7**. ST's own STM32duino
> variant file agrees (`D0 = PB7`, and `PA3` "could be on D0"). The table above
> carries the corrected value.

Five spare I/Os remain (PA6, PA7, PC0, PB8, PB5) if a later build needs them.

### Do not route these

Every one of these is physically reachable on CN10, so the front-end has to avoid
them by intent rather than by geometry:

| Position | Signal | Why |
|---|---|---|
| CN10-2, -4, -38 | PC4, PC5, PC3 | **RF switch** FE_CTRL1/2/3 — the radio owns them |
| CN10-22 | PB0 | **VDD_TCXO** — the radio's reference supply |
| CN10-35, -37 | PB6/PA2, PB7/PA3 | **ST-LINK VCP** — the debug log |
| CN10-7 | AVDD / VREF+ | **Voltage reference.** Loading it skews `battery_read_mv()` |
| CN10-32 | AGND | Analog ground — do not use as a signal return |
| CN10-31 | PB3 | Also **TRACESWO**; avoid if you ever want trace |
| CN10-1, -36, -12 | PA0, PA1, PC6 | User buttons B1/B2/B3 — switch + pull-up already fitted |
| CN10-26, -28, -30 | PB9, PB15, PB11 | User LEDs — an LED across a 1-Wire line ruins it |
| CN7-13/15 | PA13, PA14 | **SWD** — leave for the debugger |

SWD at CN7-13/15 is reachable, so the front-end may carry debug out to a test
header if you want one — just do not route it through the CN10 ribbon.

### The CN6 power tap

| Pin | Name | Use |
|---:|---|---|
| 1 | NC | — |
| 2 | IOREF | — |
| 3 | NRST | — |
| 4 | **3V3** | ✅ **battery in** |
| 5 | 5V | — |
| 6 | **GND** | ✅ battery return |
| 7 | **GND** | ✅ battery return |
| 8 | VIN | 7–12 V input, unused |

Use a **1×8 socket spanning the whole header** with only positions 4, 6 and 7
wired. A shorter socket can slide along the strip and put `VBAT` on `NRST` or
`5V`; a full-length one cannot. `CN7-16` is the same 3V3 net if you would rather
tap power from the morpho side — but then use a full-length socket there too, or
you are back to a connector that can slip.

---

## 3. Sensor front-end circuit

Two independent sensing domains share the one gated rail: the **1-Wire probes**
(off-board, on cable) and the **I2C air sensors** (on-board). They have nothing in
common electrically beyond `VSENS` and GND.

### The 1-Wire probes — six identical channels

One point-to-point line per probe. **No shared bus**, and that is a design
decision, not an accident of history:

- It is what makes the driver's **SKIP ROM** legal — no ROM search, no ROM codes
  to record, no "which address is the inlet probe?" problem. Probe identity is
  simply which connector it is plugged into.
- All six convert **in parallel**: `main.cpp` starts every conversion, waits once,
  then reads. Six probes cost one 750 ms conversion, not six.
- **A shorted or flooded probe takes down only itself.** On a shared bus it would
  take the other five with it — which, on a pole in a field, is the difference
  between one bad reading and a dead node.

The cost is six pins (there were 15 spare) and six sets of passives — three parts
per channel on the PCB, one at the probe:

As a connection list, because every ASCII schematic of this ends up drawing the
100 nF's leg across the DQ line, and that reads as a junction it is not:

```
   ONE channel, n = 0..5.  Repeat SIX times.
   "DQ node" is the front-end net between the series resistor and the connector.

   ON THE FRONT-END PCB
     VSENS      --[2.2k]--   DQ node        pull-up, on the SWITCHED rail
     MCU DQ_Pn  --[100R]--   DQ node        series damping
     DQ node    --[TVS ]--   GND            bidirectional, <50 pF

   ACROSS THE CABLE   (3 conductors; DQ twisted with GND)
     VSENS      -----------  probe V   (VDD)
     DQ node    -----------  probe DQ  (data)
     GND        -----------  probe G   (GND)

   AT THE PROBE  (far end of the cable)
     probe V    --[100nF]--  probe G        <-- VDD to GND.
                                                NOT to DQ. See below.
```

**Per channel that is three parts on the PCB and one at the probe:** one 2.2 k
pull-up, one 100 Ω series resistor and one TVS on the board, plus **one 100 nF at
the far end of the cable, across that probe's VDD and GND pins**.

> **The 100 nF never touches DQ, and putting it there would kill the bus — not
> degrade it, kill it.** 100 nF against the 2.2 k pull-up is an RC of **220 µs**.
> A 1-Wire read slot needs the line back high within about **9 µs** of release, by
> which point it would have reached **4 % of VDD**; even the 480 µs reset's
> presence window at 70 µs only reaches 27 %. Nothing would ever answer. For
> scale, this document rations the *TVS* on that same node at **<50 pF** — 100 nF
> is **2000×** that budget. The only capacitance DQ is allowed is what the cable
> unavoidably brings, which is why the pull-up is 2.2 k rather than 4.7 k.

### How many capacitors, and where

The 100 nF above is the one that trips people up, because it is the only channel
part that lives **off** the PCB. Counting the whole front-end:

| Ref | Value | Qty | Where it goes |
|---|---|---:|---|
| *(no ref — not on the PCB)* | **100 nF** | **6** | **At the probe**, one per probe, across that probe's VDD/GND at the far end of the cable |
| C2 | 100 nF | 1 | On the PCB — `VSENS` decoupling |
| C4–C6 | 100 nF | 3 | On the PCB — one at each SHT45 |
| C7 | 100 nF | 1 | On the PCB — at the SCD41 |
| C8 | 100 nF | 1 | On the PCB — at the TCA9548A bus switch |
| C1 | 1–10 µF | 1 | On the PCB — `VSENS` bulk (bounded by `DS_POWER_SETTLE_MS`, § below) |
| C3 | 10 µF | 1 | On the PCB — SCD41 local bulk, close to the part |

So **twelve 100 nF parts in the design: six out at the probes, six on the board**
— and the six probe ones are bought with the probe harnesses, not with the PCB
assembly. Going from one SHT45 to three added two of the on-board six; the mux
added the third.

**Are the six in parallel? On paper yes, in practice no — and the difference is
the whole point.** All six do sit between the same two nets (`VSENS` and `GND`),
so at DC they sum to 600 nF. But each one is separated from the board and from
the other five by its own cable run: order **0.5 µH and 0.08 Ω per metre** for
24 AWG twisted pair, so a 20 m probe sits behind roughly **10 µH and a few ohms**
round trip. At the frequencies a DS18B20's supply transient cares about, that
series impedance isolates them completely. They are six *local* reservoirs, not
one 600 nF bulk capacitor.

Which is exactly why they are specified at the probe:

| | Six 100 nF at the probes | Six 100 nF lumped on the PCB |
|---|---|---|
| Net list | identical | identical |
| Part count / cost | identical | identical |
| What the DS18B20 sees | its own reservoir, ≤100 mm away | 20 m of wire, then a cap |
| Result | works | 600 nF of pointless board bulk |

Put them across the probe's own VDD/GND **pins**, at the sensor end of the cable.
A 100 nF sitting on the PCB instead does nothing for a probe 20 m away.

They are also **per probe, not per board**: fit four probes and you fit four
caps. Nothing on the front-end changes.

A probe that is not fitted needs nothing populated but is harmless if it is: an
unconnected line sees no presence pulse, is transmitted as the invalid sentinel,
and is dropped by the gateway rather than appearing as a fake 0.00 °C.

### The I2C sensors — three SHT45s and one SCD41

All four parts mount **on the front-end board**, not on cables — they measure the
enclosure's air, and a long I2C run outside the box would be another thing to go
wrong. The three SHT45s read three points in the box, so a gradient (or a dead
sensor) is visible instead of being averaged away by a single probe.

#### Why there is a bus switch

**The SHT4x I2C address is fixed at the factory by the order code.** `SHT45-AD1B`
is 0x44 and there is no address pin to strap, so three of them cannot share a
bus. This is the one place where three of a part is harder than one.

**Power-gating them individually does not solve it**, which is worth writing down
because it is the obvious idea and it fails quietly. An unpowered SHT45 still has
ESD clamp diodes from SDA and SCL to its own VDD pin. The 4.7 k bus pull-ups
charge a gated-off part's rail through those diodes to about **3.3 − 0.6 = 2.7 V**,
and the SHT4x runs from **1.08 V** — so it wakes up and answers at 0x44 anyway.
Adding a pull-down on the gated rail does not rescue it either:

| Pull-down on the gated VDD | Gated-off part sits at | Result |
|---|---|---|
| none (float) | ~2.7 V | alive, answers at 0x44 — collision |
| 10 kΩ | ~1.8 V | still above 1.08 V — still alive |
| 1 kΩ | ~0.5 V | part dead, but SDA clamped near **1.1 V**, below the 2.31 V VIH — bus stuck low |

There is no value that both kills the part and leaves the bus valid. **What has
to be switched is the bus, not the power.**

So the three SHT45s sit behind a **TCA9548A** (or PCA9548A) bus switch, one
channel connected at a time. It is itself an I2C device, so it costs **no MCU
pins and no connector signals**. The SCD41 is at 0x62, collides with nothing, and
stays **upstream** of the switch where it needs no channel selection:

Drawn as a net list rather than as art — two devices on one bus is a case where
every ASCII rendering ends up crossing wires it does not connect:

```
   UPSTREAM  (the MCU's bus)
     VSENS ─┬─ [4.7k] ─── SDA (PA11) ─── U3 mux SDA + U2 SCD41 SDA
            ├─ [4.7k] ─── SCL (PA12) ─── U3 mux SCL + U2 SCD41 SCL
            ├─ U3 TCA9548A VDD   0x70, with [100nF] to GND at the part
            └─ U2 SCD41 VDD      0x62, with [100nF] + [10uF] to GND at the part

   DOWNSTREAM  (one channel connected at a time)
     U3 ch0 SD0/SC0 ─── U1a SHT45 #0   0x44, [4.7k] SD0/SC0 pull-ups, [100nF]
     U3 ch1 SD1/SC1 ─── U1b SHT45 #1   0x44, [4.7k] SD1/SC1 pull-ups, [100nF]
     U3 ch2 SD2/SC2 ─── U1c SHT45 #2   0x44, [4.7k] SD2/SC2 pull-ups, [100nF]

   GND ─────┬─ U1a GND  ├─ U1b GND  ├─ U1c GND  ├─ U2 GND  └─ U3 GND
```

Both pull-ups go to `VSENS`, not to permanent 3V3 — same rule as the six 1-Wire
ones. Decoupling here is **two** 100 nF (one per part) plus the SCD41's 10 µF
local bulk; these are on the PCB and are entirely separate from the six 100 nF
that live out at the probes.

| Part | Address | On | Supplies | Notes |
|---|---|---|---|---|
| U1a SHT45 | `0x44` | mux ch0 | air temp 0, humidity 0 | Frame slot 0 — the historic `air_temp`/`humidity` series. Put this one at the **vent** |
| U1b SHT45 | `0x44` | mux ch1 | air temp 1, humidity 1 | `air_temp_2` / `humidity_2` |
| U1c SHT45 | `0x44` | mux ch2 | air temp 2, humidity 2 | `air_temp_3` / `humidity_3` |
| U2 SCD41 | `0x62` | upstream | CO2 | **~205 mA peaks.** Its own 10 µF local bulk, short and wide to GND |
| U3 TCA9548A | `0x70` | upstream | — | 8-channel bus switch; A2/A1/A0 to GND. Only 3 channels used |

**Each downstream channel needs its own pull-ups.** The mux passes signals, not
the pull-ups: a channel with none floats and the SHT45 on it never answers. That
is three more pairs of 4.7 k, all on `VSENS` like everything else.

#### Wiring it

A net list, not a schematic: two devices on a switched bus is exactly the shape
that makes ASCII art draw crossings it does not mean. Every line below names one
net and everything on it.

```
  VSENS  ─┬─ R15 4k7 ── SDA_UP          upstream pull-ups
          ├─ R16 4k7 ── SCL_UP
          ├─ R23 10k ── U3.RESET        active-LOW: must NOT float
          ├─ R17 4k7 ── SD0   ┐
          ├─ R18 4k7 ── SC0   │
          ├─ R19 4k7 ── SD1   │  ONE PAIR PER CHANNEL. The mux passes
          ├─ R20 4k7 ── SC1   │  signals, not pull-ups: a channel with
          ├─ R21 4k7 ── SD2   │  no pair of its own floats, and that
          ├─ R22 4k7 ── SC2   ┘  SHT45 never answers.
          ├─ U3.VCC     + C8 100nF to GND     TCA9548A
          ├─ U2.VDD     + C7 100nF to GND     SCD41   (upstream)
          ├─ U1a.VDD    + C4 100nF to GND     SHT45 #0
          ├─ U1b.VDD    + C5 100nF to GND     SHT45 #1
          └─ U1c.VDD    + C6 100nF to GND     SHT45 #2

  SDA_UP ─┬─ PA11  (CN10-5)             SCL_UP ─┬─ PA12  (CN10-3)
          ├─ U2.SDA                             ├─ U2.SCL
          └─ U3.SDA                             └─ U3.SCL

  SD0    ─┬─ U3.SD0                     SC0    ─┬─ U3.SC0
          └─ U1a.SDA                            └─ U1a.SCL
  SD1    ─┬─ U3.SD1                     SC1    ─┬─ U3.SC1
          └─ U1b.SDA                            └─ U1b.SCL
  SD2    ─┬─ U3.SD2                     SC2    ─┬─ U3.SC2
          └─ U1c.SDA                            └─ U1c.SCL

  GND    ─┬─ U1a.VSS, U1b.VSS, U1c.VSS, U2.GND, U3.GND
          ├─ U3.A0, U3.A1, U3.A2      all three LOW = address 0x70
          └─ C4, C5, C6, C7, C8 low side

  open   ─── U3.SD3..SD7, U3.SC3..SC7   five unused channels, leave unconnected
```

Pin **names** above are what to wire to; pin **numbers** differ between the
TCA9548A's TSSOP-24 and QFN-24 packages, so take those from the datasheet for the
part you actually buy. For reference, the SHT4x DFN-4 is `1 = SDA`, `2 = VSS`,
`3 = VDD`, `4 = SCL` — confirm against your footprint before fabricating, because
all three sensors share it and a mirrored footprint is three mistakes, not one.

#### Five ways this goes wrong

1. **`RESET` left floating.** It is active-LOW with no internal pull-up, so a
   floating pin means the switch may sit in reset and *nothing* downstream ever
   answers — while the SCD41 upstream works perfectly, which sends you hunting in
   the wrong place. R23 to `VSENS` fixes it; a direct tie to `VSENS` is acceptable
   if you never want to reset it from firmware.
2. **`A0`/`A1`/`A2` left floating.** The address is then undefined and the switch
   answers at something other than 0x70, or intermittently. Tie all three to GND.
3. **Missing downstream pull-ups.** The single most likely fault on this board.
   The mux is a set of analog switches: it passes SDA and SCL through, but the
   pull-ups do not propagate. A channel wired with sensor but no resistors scans
   as empty, exactly like a dead sensor.
4. **An SHT45 accidentally wired upstream.** It then collides with the other two
   the moment a channel opens. Bring-up step 8 catches this: with all channels
   closed you must see `0x62` and `0x70` and **no** `0x44`.
5. **Any of it on permanent 3V3 instead of `VSENS`.** Same rule as everything else
   on this board — including R17–R22, which are easy to forget because they sit
   on the far side of the switch.


> **Which sensor is which is decided by the channel, not by the part.** Sensor
> index = mux channel = frame slot = dashboard metric name, wired together by
> `I2C_MUX_CHANNELS` in `node_config.h`. Three identical 0x44 parts are otherwise
> indistinguishable, so **silkscreen the channel number next to each one**.

Two placement rules that are easy to get wrong:

1. **No SHT45 may sit downwind of the SCD41 or over a copper pour that reaches
   the gate FET.** They are the accurate sensors on the board (±0.1 °C); mounting
   one next to a part that dissipates 205 mA in bursts throws that away. With
   three of them this is now a layout constraint on the whole board rather than
   on one part — spread them, and give each a cut-out or slots.
   **Separate them from each other too.** Three sensors clustered in one corner
   measure one point three times, which is an expensive way to buy nothing. The
   reason to fit three is to see a gradient: put #0 at the vent (it is the one
   whose history is continuous with the old single-sensor series), #1 mid-box,
   #2 at the far end.
2. **The SCD41 needs the same air the plants do.** Both parts want the enclosure
   vented — a sealed IP65 box measures the CO2 of its own interior, which is a
   number that means nothing. Use a **vented gland or a Gore-type membrane vent**,
   which keeps the IP rating while letting gas equilibrate.

### The rail gate

Upstream of **all** channels — six probes and all four I2C parts (three SHT45s
and the SCD41), plus the bus switch that fans the SHT45s out:

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
| DQ pull-up ×6 | **2.2 kΩ** | For the 10–20 m run. 4.7 kΩ is the short-bus value; 2.2 kΩ buys back rise time against cable capacitance. Keep ≥1.5 kΩ so the DS18B20 can still pull a valid low |
| DQ series ×6 | **100 Ω** | Reflection damping, and the sacrificial element ahead of the TVS |
| TVS ×6 | bidirectional, **<50 pF**, V_RWM **≥ 3.6 V** | 20 m of wet cable is an antenna. Capacitance is the spec that matters — high-C protection rounds off the bit slots. See *Choosing the TVS* below: this rules out most parts you will find by searching "TVS" |
| Cap at probe ×6 | **100 nF** | At the **far** end across the probe's VDD/GND, not on the PCB |
| Bulk on `VSENS` | **1–10 µF** | Bounded by the settle time below |
| I2C pull-ups | **4.7 kΩ** | On `VSENS`, like the 1-Wire ones. 100 kHz over a few cm of board wants nothing faster. **Four pairs**: one upstream, one per mux channel |
| SCD41 local bulk | **10 µF** | Handles its 205 mA current steps close to the part, so they do not appear on `VSENS` |

> **The six pull-ups do not cost standby current.** They idle high with the line
> released and only sink current while a device or the MCU pulls low — a few
> milliamps for a few milliseconds per wake. They *are* six more parts on the
> switched rail, which is the second reason (after leakage) they must sit on
> `VSENS` and not on permanent 3V3.


#### Choosing the TVS

This one part has two easy ways to get wrong, and both of them break the bus
rather than merely protecting it badly.

**It is not a rectifier.** The `SS` series — SS110, SS120, SS210, SS220, SS310,
SS320, SS510, SS515, SS520 — are **Schottky rectifiers**, decoded as
`SS<amps><volts/10>`; the SMA/SMB/SMC packaging is what makes them look like
candidates. Fitting one here fails three ways at once:

- **It never clamps.** A 100 V rectifier does nothing on a 3.3 V line until long
  after the DS18B20 (absolute max +6.0 V on DQ) and the MCU pin are destroyed.
- **Capacitance.** Junction capacitance scales with die area, so it grows with the
  current rating: even a 1 A part is tens-to-hundreds of pF, and the 5 A parts are
  far worse. Anything near 50 pF rounds off the 1-Wire bit slots on a long run.
- **Reverse leakage at temperature**, the one that hurts most. Schottky leakage
  roughly doubles per 10 °C. The TVS sits DQ→GND while DQ idles high through
  2.2 kΩ: tens of µA is 22 mV and harmless, but a milliamp at 60–70 °C board
  temperature is 2.2 V of droop and the line never reaches a valid high. That is a
  node which passes on the bench and dies in the sun.

**It is also not a general-purpose TVS.** Low-voltage power TVS — SMAJ5.0CA,
SMBJ5.0CA, SMF5.0CA and friends — are typically several hundred to over 1000 pF,
because a low breakdown voltage means a large die. They would kill the bus for
exactly the same reason the Schottkys do.

What is wanted is the **signal-line / ESD-array** class:

| Parameter | Value |
|---|---|
| Type | bidirectional TVS, explicitly sold as **low-capacitance** |
| Working standoff `V_RWM` | **≥ 3.6 V** — `VSENS` reaches 3.6 V on a fresh cell, so a 3.3 V part is marginal. The 5 V class is the practical pick |
| Clamping voltage | as low as available; R9–R14's 100 Ω covers the residual |
| Capacitance | **≤ 50 pF**, and ≤ 15 pF is easy to buy |
| Package | SOD-323 / SOT-23 |

Note how loose the capacitance budget actually is. 1-Wire bit slots are ~60 µs;
this is not USB. Anything in the ESD-protection class clears 50 pF with room to
spare, so **do not pay for 0.3 pF** — spend the selection effort on `V_RWM` and on
leakage instead.

Three parts checked against this spec (prices/stock LCSC, Aug 2026):

| Part | `V_RWM` | C typ | Package | Clamp | Leakage | Price | Verdict |
|---|---|---|---|---|---|---|---|
| Bourns **CDSOD323-T05LC** | 5.0 V | 1.0 pF | SOD-323 | 9.8 V @ 1 A, 18.3 V @ 15 A | 5 µA max | ~$0.47–0.71 | **Recommended.** Hand-solderable, 350 W, huge margin on every axis |
| onsemi **ESD9B5.0ST5G** | 5.0 V | 15 pF | **SOD-923** | 12.5 V | low | ~$0.018 | **Budget pick.** 30× cheaper and 15 pF is still fine — but SOD-923 is 0.8 × 0.6 mm. Fine for an assembly service, unpleasant by hand |
| BORN **BSD3C031L2** | **3.3 V** | 1.4 pF | SOD-323 | 17 V | 200 nA | ~$0.06 | **Rejected — and it is the instructive one.** Everything else about it is good, but `VSENS` reaches **3.6 V** on a fresh cell, above its rated standoff. Its 200 nA leakage is only guaranteed at 3.3 V |

That last row is exactly the trap the `V_RWM ≥ 3.6 V` line exists to catch: a 3.3 V
part looks like a bargain right up until a fresh cell biases it past its rating
and it starts loading the bus.

> **A quick way to sort them:** a signal-line TVS datasheet puts capacitance in
> the headline specifications. A power TVS datasheet buries or omits it. If you
> have to hunt for the pF figure, it is the wrong class of part.

**What the board-side TVS actually protects.** It protects the **MCU**, not the
probe. The DS18B20 sits 20 m away behind roughly 10 µH of cable inductance, which
at surge di/dt is an effective open circuit — nothing on this PCB defends it, and
there is nowhere sensible to put protection at a potted 3-wire probe. That is an
accepted trade: the probe is cheap and replaceable, the Nucleo is not.

**On the clamping voltage.** 18.3 V at 15 A is far above both the DS18B20's +6.0 V
absolute maximum and the MCU's tolerance, and that is fine — it is what R9–R14 are
for. The 100 Ω in series limits injected current into the MCU's internal clamps to
roughly (18.3 − 3.8) / 100 ≈ 145 mA at that worst-case pulse, and ~60 mA at the
1 A clamp point. If you want more margin, **220 Ω works too**: the DQ low level
becomes 3.3 − (3.3/2420) × 2200 ≈ **0.3 V**, still comfortably under the DS18B20's
0.3 × VDD threshold, and the injected current halves. Do not go much beyond that
or the low level stops being convincing.

**Six discretes, not an array.** A 4- or 6-line array would save board area, but
rail-clamp arrays reference a supply pin, which on this board is the *switched*
`VSENS` — one more thing to reason about at gate-off, for no gain on a board that
has plenty of room. Six identical two-pin parts also keep the channels genuinely
independent, which is the same reasoning that gave each probe its own GPIO.

### The settle-time constraint

There are **two** settle windows, and they pull in opposite directions:

| Constant | Value | What must be true by then |
|---|---|---|
| `DS_POWER_SETTLE_MS` | **10 ms** | `VSENS` fully up, so the first 1-Wire reset sees a valid bus |
| `I2C_POWER_SETTLE_MS` | **1000 ms** | The SCD41 will accept a command (its own datasheet power-up time) |

The firmware overlaps them — it starts all six DS18B20 conversions at 10 ms and
spends the SCD41's remaining ~990 ms covering the 750 ms conversion, so the rail is
on for the **longer** of the two, not the sum. Going from two probes to six does
not change this: the extra bit-banged traffic (~7 ms per probe) hides inside the
same wait. The board only has to satisfy the 10 ms figure.

Keep total `VSENS` bulk at **≤10 µF plus the SCD41's local 10 µF** and put
**100 Ω–1 kΩ in series with the gate** to tame inrush — that lands around a
millisecond. If you raise the bulk further to quiet the SCD41's current steps,
raise `DS_POWER_SETTLE_MS` to match; the two are one design decision.

**Count the probe caps in that budget.** They are decoupling where they sit, but
the gate still has to charge them through the cable: six × 100 nF = **0.6 µF**
added to `VSENS`. Against C1's 1–10 µF that is 6–40 % — small, real, and it grows
if you ever add probes or lengthen cables. The cable resistance slows that charge
rather than the FET, but 10 ms has ample margin for 0.6 µF through a few ohms.

> Oversize that cap without touching the constant and the failure mode is
> intermittent CRC errors on the first read after each wake — and with six probes
> you now get to guess which of the six is "the flaky one". Miserable to diagnose
> in the field.

### On the P-FET

The CO2 sensor set what this part has to do, and six probes did not change it:

1. **Continuous drain current and R_DS(on)** at 3.3 V of gate drive. The load is
   the SCD41's ~205 mA plus ~9 mA of probes — an AO3401-class part at ~50 mΩ costs
   about 11 mV of drop, comfortable, but not so overkill that it goes without
   checking.
2. **Drain-source leakage (I_DSS)**, still the parameter that matters most: at a
   15-minute duty cycle the part is off 99.9 % of the time. At ~1 µA it is the same
   order as the STM32's Stop2 current, i.e. the power gate becomes a real fraction
   of the battery budget. A low-leakage P-FET, or a load-switch IC with a spec'd
   sub-100 nA off current, is the upgrade.
3. **Inrush into the larger bulk.** More capacitance on `VSENS` means a bigger
   turn-on surge through the same FET; the gate series resistor is what limits it.

Route `VBAT` → Q1 → `VSENS` as a **wide** trace, and give the SCD41 a short, fat
GND return. 205 mA down a thin trace is a visible droop on the rail that the
SHT45 and all six DS18B20s share. On a large board it is tempting to let `VSENS`
wander to reach six connectors — don't; run it as a spine with short stubs.

---

## 4. Bill of materials — front-end PCB

| Ref | Part | Value / spec | Notes |
|---|---|---|---|
| J7 | Box header 2×19, shrouded | 2.54 mm, keyed, latching | Signal cable to Nucleo CN10 |
| J8 | Battery / power connector 2-pin | keyed, latching (JST-XH, Micro-Fit 3.0) | `VBAT` + `GND` to Nucleo CN6-4/6 |
| J1–J6 | Pluggable screw terminal, 3-pin | Phoenix MC 1,5/3-ST-3,5 or clone | **One per probe** — six of them |
| Q1 | P-MOSFET SOT-23 | A0341 / AO3401-class, Vgs(th) ≤ −1.5 V, ≥0.5 A | High-side gate; check I_DSS **and** that it carries the SCD41's 205 mA peaks |
| R1 | Resistor 0805 | 100 kΩ | Gate→source pull-up — **holds the rail off at reset** |
| R2 | Resistor 0805 | 100 Ω–1 kΩ | Gate series, inrush limit |
| R3–R8 | Resistor 0805 | 2.2 kΩ | **Six** DQ pull-ups, **on `VSENS`** |
| R9–R14 | Resistor 0805 | 100 Ω | **Six** DQ series |
| D1–D6 | TVS bidirectional, **low-capacitance signal-line type** | Bourns **CDSOD323-T05LC** (SOD-323, 5 V, 1 pF) or onsemi **ESD9B5.0ST5G** (SOD-923, 5 V, 15 pF) | **One per DQ line.** `V_RWM` must be ≥ 3.6 V — see *Choosing the TVS* in §3 for why a 3.3 V part fails here |
| C1 | Ceramic | 1–10 µF | `VSENS` bulk (see settle time) |
| C2 | Ceramic | 100 nF | `VSENS` decoupling |
| U1a, U1b, U1c | **SHT45** ×3 | I2C `0x44`, DFN | Air temp + humidity. All three are the SAME address — they are separated by U3, not by addressing. **Vented, spread across the board, away from U2 and the FET** |
| U2 | **SCD41** | I2C `0x62` | CO2. Needs vented air and its own local bulk. Sits **upstream** of U3 |
| U3 | **TCA9548A** | I2C `0x70`, 8-ch bus switch | What makes three 0x44 parts possible. A2/A1/A0 to GND. Only ch0–ch2 used |
| R23 | Resistor 0805 | 10 kΩ | `RESET` pull-up to `VSENS`. **Not optional** — `RESET` is active-LOW with no internal pull-up, and a floating pin can hold the switch in reset |
| R15, R16 | Resistor 0805 | 4.7 kΩ | **Upstream** I2C SDA/SCL pull-ups, **on `VSENS`** |
| R17–R22 | Resistor 0805 | 4.7 kΩ | **Downstream** pull-ups — one pair per mux channel, **on `VSENS`**. The mux does not pass pull-ups; a channel without them never answers |
| C3 | Ceramic | 10 µF | SCD41 local bulk, close to the part |
| C4–C8 | Ceramic | 100 nF | Decoupling: one per SHT45 (×3), one at the SCD41, one at U3 |
| — | 38-way ribbon + 2× IDC 2×19 socket | 2.54 mm, ≤30 cm | One socket is the Nucleo end; **clip CN10-6 and plug the matching hole** |
| — | IDC key plug | — | The rotation key (§1) |
| — | 1×8 socket + 2-core lead | 2.54 mm | CN6 power tap, positions 4/6/7 wired |
| — | M3 nylon standoffs + screws | ×8 | Both boards |
| — | Cable ties / strain relief | ×2 | Within 30 mm of each connector |
| TP1–12 | Test pads | — | `VSENS`, **all six DQ**, `SENS_GATE`, `VBAT`, `GND`, upstream SDA, SCL |
| TP13–18 | Test pads | — | The three downstream SDA/SCL pairs. Without these, a dead SHT45 and a dead mux channel look identical |

At the probe end (not on the PCB): **100 nF** across each probe's VDD/GND — six of
them.

---

## 5. Power — three traps

**The ST-LINK will eat the battery.** A Nucleo's debug section draws milliamps,
fatal for cells. Either feed 3V3 directly to the target at **CN6 pin 4** — UM2592
Table 9 names `CN6 pin 4` and `CN7 pin 16` as the same 3V3 input and rates it at
**3 V to 3.6 V, 1.3 A**, and states plainly that "the programming and debugging
features are not available, since the ST-LINK is not powered" — or **snap off the
ST-LINK section**; these boards are scored for it. Then measure: if Stop2 current
is not single-digit µA, something on the board is still alive.

> **The morpho's `VBAT` (CN7-33) is a trap.**
> It is the STM32's **RTC backup** pin, *not* a power input — it cannot run the MCU
> or the radio, and the signal ribbon runs right past it. Feed power at **CN6-4**
> (or CN7-16), never at CN7-33.

**Battery chemistry is constrained by the DS18B20, not the MCU.** The STM32WL runs
to 1.8 V but the **DS18B20 needs ≥3.0 V**, so 2×AA alkaline (sagging to ~2.0 V)
kills the sensors long before the radio quits. Use **LiFePO4 (3.0–3.6 V)** or
**2× lithium AA (L91)** — the rail `battery.cpp` already assumes.

**The CO2 sensor sets the peak-current requirement, and it is not close.** The
SCD41 pulls **~205 mA** during a measurement, against the LoRa TX's ~150 mA peak
and the six DS18B20s' ~9 mA combined. Two consequences:

- **Cell internal resistance matters.** A cell that sags below 3.0 V under a
  200 mA pulse browns out the DS18B20 rail — and because the CO2 measurement
  happens *after* the water temperatures are read, the symptom is a node that looks
  fine for months and then starts resetting as the cells age. Check the cell's
  pulse-load spec, not just its capacity.
- **The measurement is not free.** A single shot blocks 5 s, and an honest reading
  needs two (the first after power-up is unsettled), so CO2 is paced by
  `CO2_EVERY_N_WAKES` in `node_config.h` — one reading per hour by default rather
  than one per wake. See README, "CO2 on a battery node", for the arithmetic.

> **Coupling to firmware:** `battery_read_mv()` reads VDDA via VREFINT. Add a
> regulator between battery and MCU and it starts reporting the regulator output
> instead of the battery — you would then need a divider plus an ADC pin added to
> the §2 contract.

---

## 6. Probe cable and enclosure

- **Probes:** 3-pin pluggable screw terminals, six of them. Field re-termination
  with cold hands is the design case; JST crimps will not survive it. Silkscreen
  `V / DQ / G` on every one, and **number them `P0`–`P5` to match `DS_PROBE_BUSES`
  and the dashboard metric names** (`temp_hot`, `temp_cold`, `temp_p2`…`temp_p5`).
  With six identical connectors this labelling is not cosmetic — it is the only
  thing that says which reading is which.
- **Cable:** one twisted pair = **DQ + GND** per probe, VDD on a separate
  conductor. Cat5 is ideal — and with six probes, one Cat5 run per **two** probes
  (2 pairs used, VDD shared) is a tidy way to leave the box.
- **Enclosure:** IP65, cable glands sized to the probe cable — **budget for six
  probe entries plus the Nucleo cable**, which is now the thing that sets the box
  size, not the boards. The front-end sits *beside* the Nucleo, so plan a flat
  footprint rather than a stacked one.
- **Venting — not optional, and three sensors make it harder.** The SHT45s and
  the SCD41 measure the air they sit in. A sealed box measures the humidity and
  CO2 of its own interior, which is a number that means nothing and drifts with
  sunlight. Fit a **Gore-type membrane vent** (keeps IP65) or a vented gland.
  With **three** SHT45s the venting matters more, not less: three sensors in a
  box with a single vent in one corner measure the vent, the middle, and a
  stagnant pocket — and only the first is the air you meant to measure. Either
  vent the box at both ends, or accept that the far sensor reads the enclosure
  rather than the crop and label it accordingly. Keep every SHT45 away from the
  SCD41 and from any copper that reaches the gate FET — both self-heat, and
  ±0.1 °C of accuracy is trivially destroyed by a few milliwatts of neighbouring
  dissipation.
- **RF:** keep the antenna clear of metal, and keep **both cables** away from the
  RF section and the CN12 SMA. A 30 cm ribbon draped over the antenna is a real
  antenna-detuning problem.
- **Test points:** the twelve in the BOM. Twelve pads turn a field failure into a
  30-second measurement, and with six probes you *will* need to ask "which one?".

---

## 7. Bring-up order

Do this before the boards go in a sealed box on a pole. The
[`bluepill_f103c8_dump`](../README.md) diagnostic is the right first power-on test
and ports to the WL55 in a few lines.

**Cable alone, nothing connected** — five minutes that pays for itself:

1. **Buzz the signal ribbon end to end** against §2's map. Confirm continuity on
   the twelve contract positions and, critically, that **nothing** rings out to
   CN10-2, -4, -7, -22, -32, -35, -37 or -38. A ribbon assembled one position out
   is the single most likely fault in this design, and this is the last moment it
   is cheap.
2. **Check the key.** With CN10-6 clipped and the socket hole plugged, the socket
   must seat one way and refuse the other. If it seats both ways, the key is not
   done — go back and do it.

**Front-end alone, cable unplugged from the Nucleo** — this is why the test pads
exist:

3. Bench supply on `VBAT`. Pull `SENS_GATE` high → `VSENS` = 0 V. Pull it low →
   `VSENS` = `VBAT`, and **all eight** signal lines (six DQ, SDA, SCL) idle high
   through their pull-ups.
4. **Pin-probe every DQ line** → `pull-up=1 pull-down=1` on all six (proves each
   pull-up is really on `VSENS`, not GND). Steps 3 and 4 catch the
   resistor-to-GND class of fault — all-zero scratchpads that pass CRC and decode
   as a convincing `0.00 °C`, which `ds18b20_read()` rejects explicitly for this
   reason.

**Connected:**

5. Gate off → sleep current is µA. With the rail off, SDA and SCL must read
   **0 V**, not a diode drop below `VBAT` — anything else means an I2C pull-up
   landed on permanent 3V3 instead of `VSENS`.
6. **Plug in ONE probe at a time**, starting at `P0`, and confirm the reading
   appears under the expected metric name before adding the next. Six probes
   plugged in at once, with one channel miswired, is a much worse debugging
   session than six one-probe checks. Warm each probe in turn by hand and watch
   the right series move.
7. All six fitted → six plausible, **independent** temperatures, CRC OK. Two
   channels that track each other exactly are two connectors wired to one MCU pin.
8. **I2C scan with every mux channel closed** → exactly `0x62` (SCD41) and `0x70`
   (TCA9548A) answer, and **no `0x44`**. A 0x44 here means an SHT45 is wired
   upstream of the mux by mistake, which will collide the moment a channel opens.
9. **Scan again with each channel open in turn** → exactly one `0x44` each time.
   Two 0x44s at once means more than one channel is open; none means that
   channel's pull-ups or its sensor are missing. Do this before trying to read
   anything: an address that does not appear is a wiring or pull-up fault, while
   an address that appears but returns CRC failures is a signal-integrity one,
   and the two want different fixes.
10. Read all three SHT45s → plausible room temperature and humidity from each,
   within ~1 °C of the DS18B20 probes if they share the air. **Then breathe on
   one of them.** Exactly one series should move. If two move together, two
   channels are bridged; if the wrong one moves, the channel-to-silkscreen
   mapping is off and every reading is mislabelled from then on.
11. Read the SCD41 → wait out its 1 s power-up, then a single shot. Outdoor air is
    **~420 ppm** and a room with a person in it runs 600–1200 ppm; a reading pinned
    near 400 on the first shot is exactly the unsettled-first-measurement artifact
    `CO2_SINGLE_SHOT_WARMUP` exists to discard.
12. **Wiggle test.** With all six reading, flex the ribbon and tug each probe lead.
    Nothing should glitch. This is the test that finds a marginal IDC crimp before
    the pole does.
13. Full cycle → wake, read, TX, sleep. Confirm the gateway logs `6/6 probes and
    3/3 air sensors reading`, confirm Stop2 current after the gate closes, and confirm the rail
    actually goes off **during** the sleep rather than at the next wake.

---

## References

- **UM2592** — STM32WL Nucleo-64 board (MB1389) user manual: the authority for
  this board. **Table 18** ("Pin assignment of the ST morpho connectors") is the
  source for §2's CN10 map and Appendix A's CN7 map; **Table 17** ("ARDUINO
  connectors pinout") for the CN6 power tap; **Table 9** ("External power sources:
  3V3") for the CN6-4 battery input; §6.6.5 and the solder-bridge tables cover the
  VCP/D0-D1 arrangement.
- Maxim/ADI **AN148** — guidelines for reliable long 1-Wire networks.
- DS18B20 datasheet — **VDD 3.0–5.5 V** is the constraint driving battery choice.
- Sensirion **SHT4x** datasheet — command set, timing, and the transfer functions
  ported in [`src/sht45.cpp`](../src/sht45.cpp).
- Sensirion **SCD4x** datasheet — command set, the 1 s power-up time, the 5 s
  single-shot duration, and the peak-current figure that sizes Q1. Ported in
  [`src/scd41.cpp`](../src/scd41.cpp).

---

## Appendix A — ST morpho CN7 (not used by the signal cable)

CN10 carries every signal (§2), so CN7 is free. It is kept here because those pins
stay exposed on the assembled node and are the natural place to hook a scope, a
bench supply or a debugger — and because the table is tedious to re-derive.

Transcribed from **UM2592 Rev 1, Table 18** and script-verified against the PDF.

| Odd | Name | | Even | Name | |
|---:|---|---|---:|---|---|
| 1 | NC | | 2 | NC | |
| 3 | NC | | 4 | NC | |
| 5 | VDD_MCU | | 6 | E5V | |
| 7 | BOOT0 | | 8 | GND |  |
| 9 | NC | | 10 | NC | |
| 11 | NC | | 12 | IOREF | |
| 13 | PA13 | ⛔ SWD | 14 | NRST | |
| 15 | PA14 | ⛔ SWD | 16 | 3V3 | alternate battery input |
| 17 | PA15 | | 18 | 5V | |
| 19 | GND |  | 20 | GND |  |
| 21 | NC | | 22 | GND | |
| 23 | PC13 | | 24 | VIN | |
| 25 | PC14 | | 26 | NC | |
| 27 | PC15 | | 28 | PB1 | |
| 29 | NC | | 30 | PB2 | spare |
| 31 | NC | | 32 | PA10 | spare — a usable 1-Wire pin, on the wrong connector |
| 33 | VBAT | ⛔ **RTC backup pin, NOT a power input** | 34 | PB4 | |
| 35 | NC | | 36 | PB14 | |
| 37 | NC | | 38 | PB13 | |

Three positions worth knowing:

- **CN7-16 `3V3`** — the same battery input as `CN6-4`, if you would rather feed
  power from this side. Use a full-length socket if you do (§2).
- **CN7-33 `VBAT`** — the STM32's **RTC backup** pin, *not* a power input. It
  cannot run the MCU or the radio. The name is the trap; do not wire a battery to
  it.
- **CN7-13/15 `PA13`/`PA14`** — SWD, the only place to get debug out. The
  front-end may bring these out to a test header.
