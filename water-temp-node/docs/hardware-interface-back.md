# water-temp-node — hardware interface spec

> ⚠ **SUPERSEDED — 2026-09.** This is the **LM5164 discrete-buck revision** of the
> spec, kept verbatim for reference. The current spec is
> [`hardware-interface.md`](hardware-interface.md), whose revision 1.0 replaces
> U6/U7 with two off-the-shelf Traco TSR 1 modules. **Do not build from this file.**
> Everything outside §5 and the power rows of §4 is identical in both.

The real (non-breadboard) build is **two PCBs joined by a cable**:

| Board | What it is | Carries |
|---|---|---|
| **Brain** | NUCLEO-WL55JC1 (off-the-shelf) | STM32WL55JC, radio, RF switch, TCXO, antenna, ST-LINK |
| **Front-end** | Large custom PCB, this spec | **6× DS18B20 probe connectors**, **3× remote-SHT45 branch connectors** behind a bus switch, **RS-485 transceiver** for the S88 CO2 head, AO3401A rail gate, pull-ups, line protection, **24 V solar input and two bucks** |

The front-end is a **large board sitting beside the Nucleo**, not stacked on it,
and the two are joined by a short cable. This document specifies that joint: the
connectors, the signals crossing them, and the front-end circuit behind them.

Everything here is a contract the **firmware already assumes**. Where a value is
forced by code, the code is cited — change one and you change the other.

- Firmware pin map: [`include/node_config.h`](../include/node_config.h)
- Gate + sleep handling: [`src/main.cpp`](../src/main.cpp)
- Wire frame (probe → metric): [`src/lora/lora_packet.h`](../src/lora/lora_packet.h)
- Cable-length guidance: [README → *Probe cable runs*](../README.md)
- Thai translation: [`hardware-interface.th.md`](hardware-interface.th.md) — **this English file is the source of truth**; update it first

---

## 1. The joint — which bus, and why

Six probe connectors, three remote-sensor branch connectors, an RS-485 head
connector, protection, two buck regulators and a 24 V solar input do not fit on a
68.6 × 53.4 mm ARDUINO outline. Once the front-end has to be its own
large board it becomes a cabled peripheral rather than anything that mounts on
the Nucleo — which is what decides the connector question.

### The recommendation

**Two cables, deliberately separate:**

| Cable | Carries | Nucleo end | Front-end end |
|---|---|---|---|
| **Signal** | `SENS_GATE`, 6× `DQ_Pn`, `SDA`, `SCL`, `S88_TX/RX`, `DBG_TX/RX`, `VBAT_SENSE`, 2× `GND` | **2×19 IDC socket over the whole of morpho CN10** | shrouded, keyed 2×19 box header |
| **Power** | `V3V3_MCU`, `GND` | **1×8 socket over the whole of ARDUINO CN6** (only pins 4/6/7 wired) | keyed, latching 2-pin (JST-XH or Micro-Fit 3.0) |

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
an unshrouded pin field *can* be pressed on **rotated 180°**. If `V3V3_MCU` were on
that ribbon, one bad insertion puts the supply rail onto a GPIO. With power on its
own keyed 2-pin lead, the worst a reversed ribbon can do is short a couple of
open-drain GPIOs to ground and cross-connect signals — unpleasant, recoverable, not
a dead board. It also keeps the supply current off a bundle carrying six
bit-banged 1-Wire lines.

**Why not loose Dupont jumpers.** Eighteen independent crimps with no latch and no
key. Each one is a field failure waiting for the first thermal cycle. If you
breadboard it that way for bring-up, do not ship it that way.

**Why not RS-485 / a differential link per probe.** Not needed at these lengths —
the probe cables are **5–10 m** and plain 1-Wire is good to 10–20 m (Maxim
**AN148**); a MAX485 *cannot* sit on a 1-Wire line at all. See README, "Probe cable runs". Past ~30 m, escalate to a
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
   │  U3 mux -> J9..J11 (3x SHT45, 5 m each)    │
   │  U4 RS-485 -> J12 (S88 CO2 head, 5 m)      │
   │  U6/U7 bucks · J14 24 V solar in           │
   │                                            │
   │   [2x19 box header, keyed]      [2-pin]    │
   └────────────┬──────────────────────┬────────┘
                │ 38-way ribbon        │ 3V3 + GND
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

**Eighteen** signals. Freeze this table; it is the interface. Every other position
on CN10 is **left unconnected** at the front-end.

Note what is *not* here: the three SHT45s add **no** signals even though they now
sit at three *different* places in the greenhouse. They are separated on the
front-end by a bus switch that is itself an I2C device (§3), so the joint does not
grow with them. Resolving their shared address with three individual power gates
instead would have cost three more — and would not have worked; see below.

> **What grew the contract from twelve to eighteen (2026-08).** The SCD41 was
> replaced by a **Senseair S88 LP**, which speaks **UART/Modbus, not I2C**, and
> wants **4.5–5.25 V** — so it brought `S88_TX`/`S88_RX`/`S88_DE`. Supply moved to a **24 V
> solar bank**, which put the MCU behind a buck and broke `battery_read_mv()`'s
> VREFINT trick, so a divider brought `VBAT_SENSE`. And with the ST-LINK
> deliberately unpowered, the debug log needs its own pins: `DBG_TX`/`DBG_RX`.
> **Older front-end boards are not forward-compatible.**

| Signal | WL55 pin | Connector | Direction | Electrical | Forced by |
|--------|----------|-----------|-----------|------------|-----------|
| `V3V3_MCU` | — | **CN6-4** (`3V3`) | in to brain | **3.3 V regulated** from the 24 V buck, ≤250 mA peak | §5 — *no longer the battery* |
| `GND` (power) | — | **CN6-6/7** | — | the supply return, its own conductor | — |
| `SENS_GATE` | **PA8** | **CN10-16** | out of brain, push-pull | active **LOW** = rail on; Hi-Z = off via 100 k | `node_config.h` `DS_PWR_*` |
| `DQ_P0` | **PA5** | **CN10-11** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_PROBE_BUSES` |
| `DQ_P1` | **PA4** | **CN10-17** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P2` | **PA9** | **CN10-19** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P3` | **PC2** | **CN10-21** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P4` | **PB8** | **CN10-27** | bidir, open-drain | 1-Wire — **moved off PC1**, which is now `S88_TX` | `DS_PROBE_BUSES` |
| `DQ_P5` | **PB10** | **CN10-25** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `I2C_SDA` | **PA11** | **CN10-5** | bidir, open-drain | I2C @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SDA_PIN` |
| `I2C_SCL` | **PA12** | **CN10-3** | out of brain, open-drain | I2C @ 100 kHz | `node_config.h` `I2C_SCL_PIN` |
| `S88_TX` | **PC1** | **CN10-23** | out of brain, push-pull | LPUART1 TX, 9600 8N1 Modbus RTU → RS-485 driver | `node_config.h` `S88_UART_*` |
| `S88_RX` | **PC0** | **CN10-14** | in to brain | LPUART1 RX ← RS-485 receiver | `S88_UART_*` |
| `S88_DE` | **PA7** | **CN10-15** | out of brain, push-pull | RS-485 driver enable: HIGH while transmitting, LOW otherwise | `node_config.h` `S88_DE_PIN` |
| `VBAT_SENSE` | **PB3** | **CN10-31** | in to brain, **analog** | divided 24 V bank voltage, 0–3.0 V — ADC1_IN2 | `battery.cpp` |
| `DBG_TX` | **PB6** | **CN10-35** | out of brain, push-pull | USART1 TX, 115200 → 3-pin debug header | `DEBUG_UART_*` |
| `DBG_RX` | **PB7** | **CN10-37** | in to brain | USART1 RX ← debug header | `DEBUG_UART_*` |
| `GND` (signal) | — | **CN10-9**, **CN10-20** | — | one net (GND); wire **both** — see *Returns vs guards* | — |
| `VSENS` | — | — | front-end internal | **gated** 3.3 V to probes, SHT45 branches, mux **and** all pull-ups | `main.cpp` `gate_on()` |
| `V5_S88` | — | — | front-end internal | **ungated** 5.1 V to the S88 head — runs continuously | §5 |

**The peak-current figure changed with the CO2 sensor.** It is now the **LoRa TX's
~150 mA**, because the S88 LP no longer shares this rail — it lives on its own
ungated 5.1 V buck output (§5), and its 300 mA peaks are drawn there, not from
`V3V3_MCU`. The six probes add ~9 mA during the conversion. Size `V3V3_MCU` for the
radio, not for the sensors.

> **`VBAT_SENSE` is the one analog conductor in the ribbon.** It sits at CN10-31,
> between LED3 (CN10-30, left open) and AGND (CN10-32, left open) — the quietest
> neighbourhood on the connector, which is why it goes there and not on a spare
> next to a bit-banged 1-Wire line. Filter it at the front-end (see §5) and sample
> it while the probes are idle.

`VSENS` is generated **on the front-end** and never crosses back to the Nucleo. The
only power crossing the joint is `V3V3_MCU` and its ground, on their own cable.
Both bucks, and the 24 V input that feeds them, live entirely on the front-end.

### Two rules the front-end must honor

**1. The pull-ups sit on `VSENS`, never on permanent 3V3.** That is **fourteen**
pull-ups — six 1-Wire, one upstream I2C pair, and one pair per mux channel (§3). On the always-on rail, each gated-off part
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

#### Returns vs guards

These are two different jobs and the ribbon carries both, so it is worth being
precise about which conductor is doing which.

A **return** is plain GND, wired at **both** ends. Signal current does not vanish
into the pin at the far end — it flows back, and it comes back along whatever
ground conductor is nearest. Give it a close one and the out-and-back loop stays
small, which is what keeps six bit-banged lines from coupling into each other.
Give it a distant one and the loop is large, which is an antenna in both
directions.

CN10 has exactly two usable GND positions, **CN10-9** and **CN10-20**, and they
are **the same net** — there is no "A" and "B" ground here, just one ground
reached at two points. Wire both. They sit where they do the most good: CN10-9
covers the I2C pair and `DQ_P0` at the low end, CN10-20 sits inside the probe
block. (CN10-32 is **AGND** and is not a substitute — pushing digital return
current through the analog ground is how you corrupt `battery_read_mv()`.)

A **guard** is different: CN10 positions **6, 10, 18, 24 and 34 are NC** on the
Nucleo, so those conductors connect to nothing at the brain end and may be tied
to GND **at the front-end end only**. Grounded at one end, a conductor cannot
carry return current at all — it works capacitively, as a screen between its two
neighbours. Useful, free, and no substitute for a real return.

| | Grounded at | Carries return current | Purpose |
|---|---|---|---|
| **Return** (CN10-9, CN10-20) | both ends | yes | keeps the current loop small |
| **Guard** (CN10-6/10/18/24/34) | front-end only | no | capacitive screen between neighbours |

> Verify with a meter that those NC positions really are open on *your* board
> before grounding them. They are NC in UM2592 Table 18, but grounding a position
> that turned out to be a GPIO is a short.

#### What to draw on J7 in the schematic

Every one of the 38 positions needs a deliberate decision, and "ground the spare
ones to be tidy" is wrong for 22 of them. Four categories:

| Positions | Count | Draw | Why |
|---|---:|---|---|
| 3, 5, 9, 11, 16, 17, 19, 20, 21, 23, 25 | 11 | **the net** | The contract signals (§2), including both GND returns |
| 6, 10, 18, 24, 34 | 5 | **GND** | NC on the Nucleo, so grounding them at this end is safe and buys a capacitive guard |
| 1, 12, 13, 14, 15, 26, 27, 28, 29, 30, 31, 33, 36 | 13 | **no-connect flag** | Real MCU pins we do not use. Float them |
| 2, 4, 7, 8, 22, 32, 35, 37, 38 | 9 | **no-connect flag** | RF switch, TCXO, AVDD, AGND, VCP, 5V_USB_CHGR — grounding any of these breaks the board |

**Never ground an unused MCU pin.** It is a GPIO: firmware that drives it push-pull
high then shorts it, and positions 26/28/30 are the user LEDs, which would sink
current continuously. Floating is correct, because those pins are configured and
owned by the Nucleo, not by this board.

**Never touch the ⛔ group, ground included.** Grounding CN10-7 (`AVDD`) shorts the
ADC reference and `battery_read_mv()` stops meaning anything; CN10-22 (`PB0`) is
the radio's TCXO supply; CN10-2/4/38 are the RF switch controls; CN10-32 is
`AGND`, a different ground that must not carry this ribbon's return current.

**Use explicit no-connect flags, not bare unconnected pins.** In KiCad or Altium
an unconnected pin and a deliberately-unconnected pin look identical on the sheet;
only the flag records intent. It also keeps ERC silent, so a *real* missed
connection still shows up as an error instead of drowning in 22 warnings you have
trained yourself to ignore.

> The one to double-check before fabricating is position **6**. It is the key
> (§1): its pin gets clipped on the Nucleo and its hole plugged in the socket, so
> the conductor is dead at the brain end either way — which is exactly what makes
> it safe to ground here. Confirm with a meter that 6, 10, 18, 24 and 34 really are
> open on *your* board before grounding them; grounding a position that turned out
> to be a GPIO is a short.

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
   [+] GND return 1 of 2    GND       9 o  o 10  NC      [g] guard
   [+] DQ_P0                PA5      11 o  o 12  PC6         B3 button
       spare                PA6      13 o  o 14  PC0         spare
       spare                PA7      15 o  o 16  PA8     [+] SENS_GATE
   [+] DQ_P1                PA4      17 o  o 18  NC      [g] guard
   [+] DQ_P2                PA9      19 o  o 20  GND     [+] GND return 2 of 2
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

| Odd | Name      | Use                    | Even | Name | Use |
|----:|-----------|------------------------|---:|--------|---|
| 1   | PA0       | button B1 — leave open | 2  | PC4    | ⛔ RF switch FE_CTRL1 |
| 3   | **PA12**  | ✅ `I2C_SCL`           | 4 | PC5    | ⛔ RF switch FE_CTRL2 |
| 5   | **PA11**  | ✅ `I2C_SDA`           | 6 | NC     | 🔑 **key: clip this pin** |
| 7   | AVDD      | ⛔ VREF+ — do not load | 8 | 5V_USB_CHGR | leave open |
| 9   | **GND**   | ✅ GND return 1 of 2   | 10 | NC    | guard (GND at front-end only) |
| 11  | **PA5**   | ✅ `DQ_P0`             | 12 | PC6   | button B3 — leave open |
| 13  | PA6       | spare                  | 14 | **PC0** | ✅ `S88_RX` (LPUART1 RX) |
| 15  | **PA7**   | ✅ `S88_DE` (RS-485 dir) | 16 | **PA8** | ✅ `SENS_GATE` |
| 17  | **PA4**   | ✅ `DQ_P1`            | 18 | NC     | guard (GND at front-end only) |
| 19  | **PA9**   | ✅ `DQ_P2`            | 20 | **GND** | ✅ GND return 2 of 2 |
| 21  | **PC2**   | ✅ `DQ_P3`            | 22 | PB0     | ⛔ VDD_TCXO |
| 23  | **PC1**   | ✅ `S88_TX` (LPUART1 TX) | 24 | NC   | guard (GND at front-end only) |
| 25  | **PB10**  | ✅ `DQ_P5`            | 26 | PB9     | LED2 — leave open |
| 27  | **PB8**   | ✅ `DQ_P4` (moved off PC1) | 28 | PB15 | LED1 — leave open |
| 29  | PB5       | spare                 | 30 | PB11    | LED3 — leave open |
| 31  | **PB3**   | ✅ `VBAT_SENSE` (ADC1_IN2) | 32 | AGND | ⛔ analog ground, not a return |
| 33  | PB12      | spare (stops a rotated socket) | 34   | NC | guard (GND at front-end only) |
| 35  | **PB6**   | ✅ `DBG_TX` (USART1 TX) | 36 | PA1    | button B2 — leave open |
| 37  | **PB7**   | ✅ `DBG_RX` (USART1 RX) | 38 | PC3    | ⛔ RF switch FE_CTRL3 |

> **An erratum in the source.** UM2592 Rev 1 prints CN10 pin 37 as "PB6 / PA3"; it
> is **PB7 / PA3**. PB6 is already at pin 35, PB7 appears nowhere else in a table
> whose own preamble says *all* MCU I/Os are on the morpho, and the note above
> that table states D0/D1 are USART1 on **PB6 and PB7**. ST's own STM32duino
> variant file agrees (`D0 = PB7`, and `PA3` "could be on D0"). The table above
> carries the corrected value.

Three spare I/Os remain (**PA6, PB5, PB12**) if a later build needs them —
and **none of them can do ADC**. `PinMap_ADC` puts ADC1 on PA10–PA15, PB1–PB4,
PB13, PB14; of those only **PB3** is on CN10 and not already committed (PA11/PA12
are the I2C pair, PA13/PA14 are SWD, the rest are on CN7 or unbonded). That is why
`VBAT_SENSE` is at PB3 and why **there is no second analog input available** — if
a later build needs one, it has to come off CN7.

### Do not route these

Every one of these is physically reachable on CN10, so the front-end has to avoid
them by intent rather than by geometry:

| Position          | Signal           | Why                                              |
|-------------------|------------------|---                                               |
| CN10-2, -4, -38   | PC4, PC5, PC3    | **RF switch** FE_CTRL1/2/3 — the radio owns them |
| CN10-22           | PB0              | **VDD_TCXO** — the radio's reference supply      |
| ~~CN10-35, -37~~  | PB6, PB7         | **Now used** — `DBG_TX`/`DBG_RX`; see note below  |
| CN10-7            | AVDD / VREF+     | **Voltage reference.** Loading it skews `battery_read_mv()` |
| CN10-32           | AGND             | Analog ground — do not use as a signal return    |
| ~~CN10-31~~       | PB3              | **Now used** — `VBAT_SENSE`. Costs TRACESWO only |
| CN10-1, -36, -12  | PA0, PA1, PC6    | User buttons B1/B2/B3 — switch + pull-up already fitted |
| CN10-26, -28, -30 | PB9, PB15, PB11  | User LEDs — an LED across a 1-Wire line ruins it |
| CN7-13/15         | PA13, PA14       | **SWD** — leave for the debugger                 |

SWD at CN7-13/15 is reachable, so the front-end may carry debug out to a test
header if you want one — just do not route it through the CN10 ribbon.

> **Why CN10-35/37 stopped being forbidden.** They were ⛔ only because they were
> assumed to be the ST-LINK VCP. They are not: **PA2/PA3 do not appear on the
> morpho at all** (check Appendix A — they are absent from CN7), and UM2592's own
> note plus ST's STM32duino variant both put **D0/D1 = USART1 on PB6/PB7** at those
> positions. The `/PA2`, `/PA3` annotations in Table 18 are the *solder-bridge
> alternative*, not the default. Since §5 now feeds `3V3` directly at CN6-4 and the
> ST-LINK is **deliberately unpowered** — "the programming and debugging features
> are not available, since the ST-LINK is not powered" — the VCP was never going to
> work in the field anyway. So the debug log takes PB6/PB7 out to its own 3-pin
> header and you attach a USB-serial adapter. **Verify the solder bridges on your
> board before laying out the PCB**; this is the one place in this document
> resting on a bridge default rather than a measurement.

> **Caveat, same shape as the SCL one.** `DBG_RX` at CN10-37 sits next to
> **CN10-38 = PC3** (RF switch FE_CTRL3). Unlike the I2C case, the debug UART *can*
> be active while the radio switches, because logging happens during TX. It is
> 115200 baud against a slow control line, so the risk is low — but do not move
> anything faster into CN10-37, and if you see corrupted log bytes only during
> uplinks, this is why.

### The CN6 power tap

| Pin | Name    | Use |
|----:|---------|-----|
| 1   | NC      | —   |
| 2   | IOREF   | —   |
| 3   | NRST    | —   |
| 4   | **3V3** | ✅ **battery in** |
| 5   | 5V      | —   |
| 6   | **GND** | ✅ battery return |
| 7   | **GND** | ✅ battery return |
| 8   | VIN     | 7–12 V input, unused |

Use a **1×8 socket spanning the whole header** with only positions 4, 6 and 7
wired. A shorter socket can slide along the strip and put `V3V3_MCU` on `NRST` or
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
     VSENS      --[4.7k]--   DQ node        pull-up, on the SWITCHED rail
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

**Per channel that is three parts on the PCB and one at the probe:** one 4.7 k
pull-up, one 100 Ω series resistor and one TVS on the board, plus **one 100 nF at
the far end of the cable, across that probe's VDD and GND pins**.

> **The 100 nF never touches DQ, and putting it there would kill the bus — not
> degrade it, kill it.** 100 nF against the 4.7 k pull-up is an RC of **470 µs**.
> A 1-Wire read slot needs the line back high within about **9 µs** of release, by
> which point it would have reached **2 % of VDD**; even the 480 µs reset's
> presence window at 70 µs only reaches 14 %. Nothing would ever answer. For
> scale, this document rations the *TVS* on that same node at **<50 pF** — 100 nF
> is **2000×** that budget. The only capacitance DQ is allowed is what the cable
> unavoidably brings: at **5–10 m** that is **250–530 pF**, which the 4.7 k pull-up
> drives to a valid high in **1.6–3.0 µs** — inside the 9 µs budget by 3×.

### How many capacitors, and where

The 100 nF above is the one that trips people up, because it is the only channel
part that lives **off** the PCB. Counting the whole front-end:

| Ref                         | Value      | Qty   | Where it goes |
|-----------------------------|------------|------:|---------------|
| *(no ref — not on the PCB)* | **100 nF** | **6** | **At the probe**, one per probe, across that probe's VDD/GND at the far end of the cable |
| C2                          | 100 nF     | 1     | On the PCB — `VSENS` decoupling |
| *(no ref — not on the PCB)* | **100 nF** | **3** | **At each SHT45**, across VDD/VSS at the far end of that branch |
| C8                          | 100 nF     | 1     | On the PCB — at the TCA9548A bus switch |
| C9                          | 100 nF     | 1     | On the PCB — at the RS-485 transceiver |
| C1                          | 1–10 µF    | 1     | On the PCB — `VSENS` bulk (bounded by `DS_POWER_SETTLE_MS`, § below) |
| *(no ref — not on the PCB)* | 100 µF + 100 nF | 1 ea | **At the S88 head**, for its 300 mA peaks |

So **eleven 100 nF parts: nine out at the sensors, two on the board** — and the
nine remote ones are bought with the sensor harnesses, not with the PCB assembly.
**This inverted in 2026-08**: the SHT45s left the board and took their capacitors
with them, and the SCD41's C3/C7 went with the part.

**Are the six in parallel? On paper yes, in practice no — and the difference is
the whole point.** All six do sit between the same two nets (`VSENS` and `GND`),
so at DC they sum to 600 nF. But each one is separated from the board and from
the other five by its own cable run: order **0.5 µH and 0.08 Ω per metre** for
24 AWG twisted pair, so a 10 m probe sits behind roughly **5 µH and 1.7 Ω**
round trip, and a 5 m probe half of that. At the frequencies a DS18B20's supply transient cares about, that
series impedance isolates them completely. They are six *local* reservoirs, not
one 600 nF bulk capacitor.

Which is exactly why they are specified at the probe:

| | Six 100 nF at the probes | Six 100 nF lumped on the PCB |
|---|---|---|
| Net list | identical | identical |
| Part count / cost | identical | identical |
| What the DS18B20 sees | its own reservoir, ≤100 mm away | 5–10 m of wire, then a cap |
| Result | works | 600 nF of pointless board bulk |

Put them across the probe's own VDD/GND **pins**, at the sensor end of the cable.
A 100 nF sitting on the PCB instead does nothing for a probe 5–10 m away.

They are also **per probe, not per board**: fit four probes and you fit four
caps. Nothing on the front-end changes.

A probe that is not fitted needs nothing populated but is harmless if it is: an
unconnected line sees no presence pulse, is transmitted as the invalid sentinel,
and is dropped by the gateway rather than appearing as a fake 0.00 °C.

### The I2C sensors — three REMOTE SHT45s

**This reversed in 2026-08.** The three SHT45s used to mount on the front-end board
and read three points *inside the enclosure*. They now sit at **three different
places in the โรงเรือน**, each on its own cable:

| Sensor | Mux ch | Location | Frame slot |
|---|---|---|---|
| SHT45 #0 | 0 | **หัวโรงเรือน** — greenhouse head | `air_temp0` / `humidity0` |
| SHT45 #1 | 1 | **ท้ายโรงเรือน** — greenhouse tail | `air_temp1` / `humidity1` |
| SHT45 #2 | 2 | **นอกโรงเรือน** — outside, ambient reference | `air_temp2` / `humidity2` |

The MCU box lives **outside** the greenhouse, because the greenhouse is hot and
humid and the electronics should not be. Only sensors go inside. Every branch is
**≤5 m**.

Sensor #0 keeps mux channel 0 because its history is continuous with the old
single-SHT45 series — see *Which sensor is which* below. **Sensor #2 is the
ambient reference** and needs a **radiation shield**, not merely an enclosure: an
unshielded sensor in Thai sun reads 10–15 °C above true air temperature and the
reference becomes worthless.

> **Why ≤5 m matters, and where it stops working.** I2C's standard-mode limits are
> **400 pF** of bus capacitance and **1000 ns** rise time. Cat5 is ~56 pF/m, and the
> SHT45 must still sink 3 mA at 0.4 V, so ~1.5 kΩ is the stiffest usable pull-up:
>
> | Branch | C_branch | 2.2 kΩ | 1.5 kΩ |
> |---|---|---|---|
> | **5 m** | ~320 pF | **0.60 µs** ✅ | 0.41 µs |
> | 10 m | ~600 pF | 1.12 µs ❌ | 0.76 µs ⚠️ |
> | 15 m | ~880 pF | 1.64 µs ❌ | 1.12 µs ❌ |
>
> At **≤5 m with 2.2 kΩ** there is 40 % margin and no extra silicon is needed. Past
> ~10 m, fit a **P82B715** I2C buffer at the box end of *that branch only* — it
> multiplies drive ~10×, needs no firmware change, and buffered and unbuffered
> branches can share the mux. Do **not** reach for PCA9615 differential I2C: it
> needs a chip at *both* ends of *every* branch, which is six ICs and power at
> three wet locations to solve what one P82B715 per branch solves.

The CO2 sensor is **no longer on this bus at all** — see *The CO2 sensor* below.

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
pins and no connector signals** — which is why three sensors at three places in the
greenhouse still fit inside the same contract. **Nothing else is on this bus:** the
SCD41 that used to sit upstream at 0x62 has been replaced by the S88 LP, which is
on RS-485 and not on I2C at all.

Drawn as a net list rather than as art:

```
   UPSTREAM  (the MCU's bus, board-local)
     VSENS ─┬─ [4.7k] ─── SDA (PA11) ─── U3 mux SDA
            ├─ [4.7k] ─── SCL (PA12) ─── U3 mux SCL
            └─ U3 TCA9548A VDD   0x70, with [100nF] to GND at the part

   DOWNSTREAM  (one channel connected at a time)
     U3 ch0 SD0/SC0 ══ 5 m cable ══ U1a SHT45 #0  0x44, [2.2k] pull-ups, [100nF]
     U3 ch1 SD1/SC1 ══ 5 m cable ══ U1b SHT45 #1  0x44, [2.2k] pull-ups, [100nF]
     U3 ch2 SD2/SC2 ══ 5 m cable ══ U1c SHT45 #2  0x44, [2.2k] pull-ups, [100nF]

   GND ─────┬─ U1a GND  ├─ U1b GND  ├─ U1c GND  ├─ U2 GND  └─ U3 GND
```

Both upstream pull-ups go to `VSENS`, not to permanent 3V3 — same rule as the six
1-Wire ones. The mux's own 100 nF is on the PCB; **each SHT45's 100 nF now lives
out at the sensor**, across its VDD/GND at the far end of that branch, exactly like
the probe capacitors and for the same reason.

> **The bus switch is now doing a second job, and it is the one that saves this
> design.** With the sensors on three separate cables, the mux means the MCU only
> ever sees **one branch's capacitance at a time**. Three 5 m branches never add up
> to a 15 m bus — each is ~320 pF on its own, which is why plain I2C still works at
> all. Had the three sensors shared one multi-drop cable, they could not (and their
> fixed 0x44 address forbids it anyway). A star of short branches behind a switch
> is the *right* topology here, not a workaround.

| Part | Address | On | Supplies | Notes |
|---|---|---|---|---|
| U1a SHT45 | `0x44` | mux ch0 | air temp 0, humidity 0 | Frame slot 0 — the historic `air_temp`/`humidity` series. **หัวโรงเรือน** |
| U1b SHT45 | `0x44` | mux ch1 | air temp 1, humidity 1 | `air_temp_2` / `humidity_2`. **ท้ายโรงเรือน** |
| U1c SHT45 | `0x44` | mux ch2 | air temp 2, humidity 2 | `air_temp_3` / `humidity_3`. **นอกโรงเรือน**, in a radiation shield |
| U3 TCA9548A | `0x70` | upstream | — | 8-channel bus switch; A2/A1/A0 to GND. Only 3 channels used |

The SCD41 that used to sit upstream at `0x62` is **gone** — replaced by the S88 LP
on RS-485. Nothing is upstream of the switch now except the switch itself, which
simplifies the **all-channels-closed I2C scan** in §7: with every channel closed you
should see **`0x70` and nothing else**.

**Each downstream channel needs its own pull-ups.** The mux passes signals, not
the pull-ups: a channel with none floats and the SHT45 on it never answers. That
is three more pairs — **2.2 kΩ now, not 4.7 kΩ**, because each pair drives 5 m of
cable rather than 30 mm of trace. All on `VSENS`, like everything else.

> **Put the downstream pull-ups at the BOARD end, not at the sensor.** They belong
> on `VSENS` so a gated-off branch cannot leak, and `VSENS` is generated on the
> front-end. A pull-up at the far end would need the rail out there anyway and
> would sit in the condensation.

#### Wiring it

A net list, not a schematic: two devices on a switched bus is exactly the shape
that makes ASCII art draw crossings it does not mean. Every line below names one
net and everything on it.

```
  ON THE FRONT-END PCB
  VSENS  ─┬─ R15 4k7 ── SDA_UP          upstream pull-ups (board-local, 30 mm)
          ├─ R16 4k7 ── SCL_UP
          ├─ R23 10k ── U3.RESET        active-LOW: must NOT float
          ├─ R17 2k2 ── SD0   ┐
          ├─ R18 2k2 ── SC0   │  ONE PAIR PER CHANNEL, and 2k2 not 4k7
          ├─ R19 2k2 ── SD1   │  because each pair drives 5 m of cable.
          ├─ R20 2k2 ── SC1   │  The mux passes signals, NOT pull-ups: a
          ├─ R21 2k2 ── SD2   │  channel with no pair of its own floats
          ├─ R22 2k2 ── SC2   ┘  and that SHT45 never answers.
          ├─ U3.VCC     + C8 100nF to GND     TCA9548A
          └─ J9/J10/J11 pin V                 3V3 out to the three branches

  SDA_UP ─┬─ PA11  (CN10-5)             SCL_UP ─┬─ PA12  (CN10-3)
          └─ U3.SDA                             └─ U3.SCL

  SD0    ─┬─ U3.SD0                     SC0    ─┬─ U3.SC0
          └─ J9.SDA                             └─ J9.SCL      -> หัวโรงเรือน
  SD1    ─┬─ U3.SD1                     SC1    ─┬─ U3.SC1
          └─ J10.SDA                            └─ J10.SCL     -> ท้ายโรงเรือน
  SD2    ─┬─ U3.SD2                     SC2    ─┬─ U3.SC2
          └─ J11.SDA                            └─ J11.SCL     -> นอกโรงเรือน

  ACROSS EACH BRANCH  (4 conductors, <=5 m; SDA twisted with GND)
     VSENS  ───────────  SHT45 VDD
     SDn    ───────────  SHT45 SDA
     SCn    ───────────  SHT45 SCL
     GND    ───────────  SHT45 VSS

  AT EACH SENSOR  (far end of the branch)
     SHT45 VDD --[100nF]-- SHT45 VSS      <-- at the SENSOR, not on the PCB

  GND    ─┬─ U3.GND, J9/J10/J11 pin G
          ├─ U3.A0, U3.A1, U3.A2      all three LOW = address 0x70
          └─ C8 low side

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
   answers. With the SCD41 gone there is now **nothing** upstream to still work
   and reassure you, so the symptom is a total I2C blackout. R23 to `VSENS` fixes it; a direct tie to `VSENS` is acceptable
   if you never want to reset it from firmware.
2. **`A0`/`A1`/`A2` left floating.** The address is then undefined and the switch
   answers at something other than 0x70, or intermittently. Tie all three to GND.
3. **Missing downstream pull-ups.** The single most likely fault on this board.
   The mux is a set of analog switches: it passes SDA and SCL through, but the
   pull-ups do not propagate. A channel wired with sensor but no resistors scans
   as empty, exactly like a dead sensor.
4. **An SHT45 accidentally wired upstream.** It then collides with the other two
   the moment a channel opens. Bring-up step 8 catches this: with all channels
   closed you must see **`0x70` and nothing else** — no `0x44`, and no `0x62`
   either, because the SCD41 is gone.
5. **Any of it on permanent 3V3 instead of `VSENS`.** Same rule as everything else
   on this board — including R17–R22, which are easy to forget because they sit
   on the far side of the switch.
6. **A branch wired with 4.7 kΩ instead of 2.2 kΩ.** This is the new one, and it is
   nasty because it *mostly* works: 4.7 kΩ over 5 m gives a 1.27 µs rise against a
   1000 ns budget, so the bus passes on the bench with a short lead and starts
   throwing intermittent NACKs once the real cable is fitted. R17–R22 are **2.2 kΩ**;
   R15/R16 upstream stay 4.7 kΩ because they only drive 30 mm of trace.


> **Which sensor is which is decided by the channel, not by the part.** Sensor
> index = mux channel = frame slot = dashboard metric name, wired together by
> `I2C_MUX_CHANNELS` in `node_config.h`. Three identical 0x44 parts are otherwise
> indistinguishable, so **silkscreen the channel number next to each one**.

**The old placement rules are gone, because the sensors left the board.** There is
no longer any question of an SHT45 sitting downwind of the CO2 sensor or over the
gate FET's copper — nothing on this PCB measures air any more. What replaces them:

1. **Each sensor needs its own small vented housing at its own location**, and #2
   needs a **radiation shield** (§6). The front-end enclosure itself no longer has
   to be vented for humidity — but see the S88 section, because the *sensor head*
   emphatically does.
2. **Silkscreen the destination, not just the channel number.** `CH0 → หัว`,
   `CH1 → ท้าย`, `CH2 → นอก` next to J9/J10/J11. Three identical connectors feeding
   three identical parts on three identical cables is exactly the situation where a
   swapped pair goes unnoticed for a season — the data still looks plausible, it is
   just attributed to the wrong end of the greenhouse.
3. **Label both ends of every branch cable.** The failure this prevents is not
   electrical; it is someone reconnecting J10 and J11 after maintenance and
   silently swapping "inside the greenhouse" with "ambient reference".

### The CO2 sensor — Senseair S88 LP, on RS-485

**This replaced the SCD41 in 2026-08 and it is not a drop-in.** Three things about
the S88 LP drive the whole rest of this document (PSP14281 Rev 7):

| Spec | S88 LP | SCD41 it replaced | Consequence |
|---|---|---|---|
| Interface | **UART, Modbus RTU** — no I2C at all | I2C `0x62` | Leaves the I2C bus entirely; needs LPUART1 + RS-485 |
| Supply | **4.5–5.25 V** | 3.3 V | Forces a second buck rail (§5) |
| Peak current | ≤300 mA | ~205 mA | Sized at the 5 V rail, not `V3V3_MCU` |
| Average current | ≤18 mA | — | 90 mW continuous — affordable on solar |
| Warm-up | <10 s | 5 s/shot ×2 | — |
| Operating range | **0–50 °C, 0–85 %RH, non-condensing** | −10–60 °C, 0–95 %RH | **The main deployment risk — see below** |
| ABC | 8-day period, **specified at continuous operation** | ASC, tolerant of single-shot | Forces continuous power |
| Protection | **unprotected against surges and reverse connection** | — | Needs external protection on a 5 m outdoor run |

#### Why it runs continuously, and why that was not optional

The datasheet's ABC (Automatic Baseline Correction) has an **8-day period**, and
states accuracy "is defined at continuous operation (at least three (3) ABC
periods … with ABC turned on)". At the old `CO2_EVERY_N_WAKES = 4` pacing the
sensor was powered ~10 s per hour — a **0.28 % duty cycle**, at which eight days of
*powered* time takes **7.8 years**. ABC would never complete once, and CO2 drift
would go permanently uncorrected.

Solar dissolves this. At 18 mA average on 5 V the S88 costs **90 mW ≈ 2.16 Wh/day**
(~2.4 Wh after buck losses), against ~80 Wh/day from a 20 W panel at four peak-sun
hours. Running it **24/7 costs about 3 %** of the daily yield, and the overnight
carry is ~1.1 Wh. So:

- `CO2_ENABLED`, `CO2_EVERY_N_WAKES` and `CO2_SINGLE_SHOT_WARMUP` are **deleted**.
- The S88 is **not** on `SENS_GATE`. Its rail is ungated and always live.
- The firmware simply reads the latest value over Modbus each wake.

> **A second, unplanned benefit.** 90 mW dissipated continuously holds the module a
> degree or two above ambient, and a degree above ambient is what keeps local
> humidity off the dew point. Duty-cycling this sensor in a greenhouse would have
> made condensation **worse**, not better.

#### The link — RS-485, and who drives the direction pin

The S88 has a **`UART_R/T` direction-control output**, documented as being for
"direct connection to RS485 receiver integrated circuit like MAX485". So at the
sensor end the S88 drives its own transceiver with no firmware involvement:

```
  FRONT-END PCB (dry box)                 SENSOR HEAD (greenhouse, 5 m)

  PC1 S88_TX ------- U4.DI                    U5.RO --- S88.UART_RxD
  PC0 S88_RX ------- U4.RO                    U5.DI --- S88.UART_TxD
  PA7 S88_DE --+---- U4.DE                    U5.DE --+- S88.UART_R/T
               +---- U4.nRE                   U5.nRE -+  (the S88 drives it)

  U4.A ========== twisted pair A =========== U5.A
  U4.B ========== twisted pair B =========== U5.B

  V5_S88 ======== 5.1 V ==================== S88.G+   + 100uF + 100nF
  GND    ======== GND   ==================== S88.G0
                                             U5.VCC <- local 3V3 LDO off 5 V
```

Three decisions are embedded there:

1. **`DE` and `!RE` are tied together and driven by one MCU pin.** Firmware raises
   `S88_DE`, writes the request, waits for the transmit-complete flag, then drops
   it — `src/s88.cpp` does this around `flush()`, which is exactly what blocks
   until the last stop bit has left the shift register. Dropping `DE` before that
   truncates the frame, which is the classic RS-485 bug; using `flush()` rather
   than a delay is what avoids it.

   > **Why not an auto-direction transceiver.** It was the first choice, because it
   > would have saved this signal and removed the bug class entirely. It does not
   > survive contact with the supply rails: the common auto-direction parts
   > (MAX13487E/MAX13488E) are **5 V** devices, and their `RO` output would drive
   > 5 V into an STM32WL pin that is **not 5 V tolerant**. Level-shifting one pin to
   > save one pin is not a trade worth making. A plain 3.3 V transceiver plus one
   > GPIO is the honest answer, and `PA7` was spare.

2. **Do not power the head transceiver from `DVCC_out`.** It is tempting: the S88
   offers a 3.3 V output right there. But the datasheet rates it at **6 mA max**
   and warns that "induced noise or excessive current drawn may affect sensor
   performance. External series resistor is strongly recommended if this pin is
   used." Use a small 3.3 V LDO off the 5 V rail instead and leave `DVCC_out`
   unloaded — as a logic reference only, if at all.
3. **No termination resistors at 5 m.** At 9600 baud the edges are microseconds and
   5 m is electrically nothing; a 120 Ω terminator would only waste current. Do
   choose a **true-failsafe** receiver (THVD1450, MAX3485E) so an idle undriven bus
   reads as a stop bit rather than as garbage — that saves the bias resistor pair
   as well.

> **`S88_DE` sits at CN10-15, between `S88_RX` (14) and `SENS_GATE` (16).** That
> looks like it breaks the "every signal gets a guard neighbour" rule, and it is
> deliberate: all three are **slow** lines (a 9600-baud direction line held for
> ~8 ms, a 9600-baud receive line, and a rail gate that changes twice per wake).
> Grouping the slow signals together is what keeps them away from the six
> bit-banged DQ conductors, which are the ones that actually need the guards.

**Protection is not optional here.** The datasheet states the part is unprotected
against surges and reverse connection, and this is 5 m of cable in a wet building.
Fit a **reverse-polarity FET on `G+` at the head**, and TVS on A/B at both ends.

#### Modbus — what is still unknown

The register map is **not** in PSP14281. Note 7 points to a separate document,
**TDE14367 "Modbus on Senseair S88"**, and that is what the driver must be written
from. What is fixed: **9600 baud, 8 data bits, 1 stop bit, no parity**, Modbus RTU
framing with CRC16. Confirm the slave address and the CO2 holding-register number
against TDE14367 before writing `src/s88.cpp` — do not assume the older S8 map
carries over.

> **Modbus CRC16 is a real advantage over what it replaced.** A corrupted I2C
> transaction produces a plausible wrong number; a corrupted Modbus frame fails its
> CRC and is simply retried. On a 5 m run through a greenhouse full of pump
> contactors, that difference matters more than the accuracy spec does.

#### The deployment risk, and the staged answer

The S88 is rated **0–50 °C and 0–85 %RH, non-condensing, dew point ≤35 °C**. A Thai
โรงเรือน routinely sits at 90–100 %RH overnight with visible condensation at dawn,
and a closed greenhouse in afternoon sun can pass 50 °C at canopy height. **Both
limits are plausibly violated daily.** An NDIR sensor with a fogged optical path
does not fail loudly — it drifts, and returns plausible wrong numbers.

The SHT45s are fine here (0–100 %RH, and they recover from condensation). This is
an S88-specific problem, and the agreed answer is **staged deployment**:

1. **Build the full board and all cabling now**, S88 branch included — connector,
   transceiver, 5 V rail, protection. Nothing needs rework later.
2. **Deploy with the three SHT45s only.** Log T/RH for a few weeks, including at the
   midpoint where the S88 will go.
3. **Fit the S88 once the logs show you are inside 0–50 °C and ≤85 %RH.** If dawn
   condensation turns out to be persistent, add a small heater resistor at the head
   before fitting the sensor.

Mount it in a **ventilated radiation shield with the diffusion area facing
downward**, so condensate drips away instead of pooling on the optical window,
behind a **PTFE/Gore membrane** that passes CO2 but not liquid water. Do not seal
it: a sealed enclosure measures the CO2 of its own interior, which is a number that
means nothing.

> **Pressure dependence is now unhandled.** The S88 drifts **1.6 % of reading per
> kPa** from normal pressure. The old design fed the SCD41 a pressure value from the
> BME280 via `scd41_set_ambient_pressure()`. There is no BME280 on the WL55 node, so
> at a fixed installation altitude this is a constant offset — acceptable, but write
> the altitude down, because a node moved from sea level to 500 m gains a ~1 %
> error that nobody will think to look for.

### The rail gate — now a recovery mechanism, not an energy measure

**Its purpose changed completely when the supply became solar.** On a battery the
gate existed to stop µA of leakage mattering over months; the 5 µA I_DSS budget
below was a real constraint. On a 24 V bank fed by a panel, that arithmetic is
irrelevant — the S88 alone draws 18 mA continuously and nobody notices.

**Keep the gate anyway, for a different reason.** There are now three unshielded
5 m I2C branches running through a greenhouse full of pump contactors, and the
characteristic I2C failure is a glitch that latches SDA low until something clocks
the bus free. Power-cycling the rail is the one recovery that always works. The
gate is now the node's **I2C and 1-Wire reset button**, and firmware should use it
that way: on a failed transaction, gate off, wait, gate on, retry.

> **What is no longer behind the gate: the S88.** Its 5 V rail is ungated and
> always live, because ABC requires continuous operation. Only the 3.3 V sensor
> rail is switched.

Upstream of the six probes, the three SHT45 branches and the bus switch that fans
them out — but **not** the CO2 sensor:

```
  U7 3V3 ─────┬──────────────── S │ Q1 (P-MOSFET) │ D ──┬──── VSENS
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
| DQ pull-up ×6 | **4.7 kΩ** | For the **5–10 m** run. Worst case (10 m of ~56 pF/m Cat5) the line reaches VIH in **3.0 µs** against the firmware's **9 µs** read-slot budget — 3× margin. Use **3.3 kΩ** only if you run shielded / high-C (~100 pF/m) cable at the 10 m end. 2.2 kΩ was the 20 m value; it still works, at 2× the pull-down current |
| DQ series ×6 | **100 Ω** | Reflection damping, and the sacrificial element ahead of the TVS |
| TVS ×6 | bidirectional, **<50 pF**, V_RWM **≥ 3.6 V** | 5–10 m of wet outdoor cable is still an antenna, and the shorter run loosens this budget without removing it. Capacitance is the spec that matters — high-C protection rounds off the bit slots. See *Choosing the TVS* below: this rules out most parts you will find by searching "TVS" |
| Cap at probe ×6 | **100 nF** | At the **far** end across the probe's VDD/GND, not on the PCB |
| Bulk on `VSENS` | **1–10 µF** | Bounded by the settle time below |
| I2C pull-ups, upstream | **4.7 kΩ** | On `VSENS`. One pair, board-local, 30 mm of trace — nothing faster needed |
| I2C pull-ups, downstream | **2.2 kΩ** | **One pair per mux channel**, on `VSENS`. 2.2 kΩ because each pair drives 5 m of cable — 4.7 kΩ misses the 1000 ns rise-time budget at that length |

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
  4.7 kΩ: tens of µA is 47 mV and harmless, but a milliamp at 60–70 °C board
  temperature would need 4.7 V across the pull-up — the line simply never reaches a
  valid high. That is a node which passes on the bench and dies in the sun. The
  weaker 4.7 kΩ pull-up makes this failure *more* sensitive than the old 2.2 kΩ did,
  not less: budget TVS leakage in µA, never mA.

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
probe. The DS18B20 sits 5–10 m away behind roughly 2.5–5 µH of cable inductance, which
at surge di/dt is an effective open circuit — nothing on this PCB defends it, and
there is nowhere sensible to put protection at a potted 3-wire probe. That is an
accepted trade: the probe is cheap and replaceable, the Nucleo is not.

**On the clamping voltage.** 18.3 V at 15 A is far above both the DS18B20's +6.0 V
absolute maximum and the MCU's tolerance, and that is fine — it is what R9–R14 are
for. The 100 Ω in series limits injected current into the MCU's internal clamps to
roughly (18.3 − 3.8) / 100 ≈ 145 mA at that worst-case pulse, and ~60 mA at the
1 A clamp point. If you want more margin, **220 Ω works too**: with the 4.7 kΩ
pull-up the DQ low level becomes 3.3 × 220 / 4920 ≈ **0.15 V** (it was 0.30 V on the
old 2.2 kΩ), still far under the DS18B20's 0.3 × VDD threshold, and the injected
current halves. Do not go much beyond that
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
| `I2C_POWER_SETTLE_MS` | **10 ms** | An SHT45 will accept a command (its soft-reset time is ~1 ms) |

**The 1000 ms window is gone with the SCD41.** That figure was the SCD41's
datasheet power-up time and it dominated every wake; the firmware had to overlap it
with the 750 ms DS18B20 conversion to avoid paying for it twice. The SHT45 needs
about a millisecond, so both settle windows now collapse to the same **10 ms**, and
the rail-on time is set purely by the **750 ms conversion**. The S88 needs no settle
window at all — it is never powered down.

Keep total `VSENS` bulk at **≤10 µF** and put **100 Ω–1 kΩ in series with the gate**
to tame inrush — that lands around a millisecond. The SCD41's separate 10 µF local
bulk is deleted along with the part. If you raise the bulk, raise
`DS_POWER_SETTLE_MS` to match; the two are one design decision.

**Count the probe caps in that budget.** They are decoupling where they sit, but
the gate still has to charge them through the cable: six × 100 nF = **0.6 µF**
added to `VSENS`. Against C1's 1–10 µF that is 6–40 % — small, real, and it grows
if you ever add probes or lengthen cables. The cable resistance slows that charge
rather than the FET, but 10 ms has ample margin for 0.6 µF through the 0.8–1.7 Ω
of a 5–10 m run.

> Oversize that cap without touching the constant and the failure mode is
> intermittent CRC errors on the first read after each wake — and with six probes
> you now get to guess which of the six is "the flaky one". Miserable to diagnose
> in the field.

### On the P-FET

The CO2 sensor set what this part has to do, and six probes did not change it:

1. **R_DS(on) at the gate drive you actually have** — and this is the one people
   read off the wrong row. Your gate drive is the battery: **3.6 V fresh, 3.0 V
   flat**, never the 4.5 V or 10 V the headline specs use. For the AO3401A the
   guaranteed point below that is **V_GS = −2.5 V → 85 mΩ max**, so at −3.0 V you
   are guaranteed at least that good. At 250 mA that is **21 mV** of drop — 0.7 %
   of a 3.0 V rail feeding a DS18B20 that needs ≥3.0 V. The 85 mΩ figure is
   measured at 2.5 A, ten times our current, so it is conservative.
   **Select on the −2.5 V row, not the −4.5 V one.**
2. **Drain-source leakage (I_DSS)**, still the parameter that matters most: at a
   15-minute duty cycle the part is off 99.9 % of the time, so leakage is billed
   continuously while everything else is billed for milliseconds.
   **This argument is now history — read it as such.** On the old battery node the
   AO3401A's 5 µA at T_J = 55 °C was **44 mAh/year** drawn whether the node woke or
   not, which mattered against a 600 mAh cell. On a 24 V solar bank it is
   **0.4 Wh/year against 80 Wh/day** — unmeasurable. The leakage line is retained
   because it explains why the part was chosen, not because it still constrains
   anything. Do not spend money on a low-leakage load switch for this node.
3. **Inrush into the larger bulk.** More capacitance on `VSENS` means a bigger
   turn-on surge through the same FET; the gate series resistor is what limits it.


**AO3401A checked against this design** (AOS datasheet Rev 3.1, Dec 2023):

| Parameter | Datasheet | This design | Margin |
|---|---|---|---|
| I_D continuous, 25 °C | −4 A | 250 mA peak | **16×** |
| I_D continuous, 70 °C | −3.2 A | 250 mA peak | 12.8× |
| R_DS(on) @ V_GS = −2.5 V | ≤85 mΩ | 21 mV drop at 250 mA | — |
| P_D, 25 °C | 1.4 W | 5.3 mW | **170×** |
| Temperature rise | θ_JA ≤125 °C/W | +0.7 °C | — |
| V_GS(th) | −0.5 / −0.9 / **−1.3 V** | needs ≤−1.5 V | ✅ |
| V_GS absolute max | ±12 V | 3.6 V | ✅ |
| **I_DSS, 25 °C** | **≤1 µA** | off 99.9 % of the time | see above |
| **I_DSS, T_J 55 °C** | **≤5 µA** | ≈44 mAh/year | the real constraint |

Current capability is a non-issue — over-specified by more than an order of
magnitude on every axis. Leakage used to be the one line worth arguing about; on
solar it no longer is.

**What the gate now carries is much less than it used to.** With the S88 on its own
ungated 5 V rail, `VSENS` feeds only the six probes (~9 mA), three remote SHT45s
(µA) and the mux — the 205 mA CO2 burst is gone from this rail entirely. Q1 is now
enormously over-specified, which is fine: it is the same part, and its real job is
now **fault recovery** (§ *The rail gate*), not current handling.

Route **U7's 3.3 V** → Q1 → `VSENS` as a wide trace anyway. On a large board it is
tempting to let `VSENS` wander to reach six probe connectors and three branch
connectors — don't; run it as a spine with short stubs. The **5.1 V** rail to J12
wants the same treatment for a better reason: it really does carry 300 mA peaks.

---

## 4. Bill of materials

Split in two, because the board is no longer the whole design: parts on the
**front-end PCB** in the dry box, and parts that live **out at the sensors**.

> **Where to buy it:** [`sourcing-th.md`](sourcing-th.md) has manufacturer part
> numbers for everything non-generic, what to check when substituting, and which
> items are better bought from local Thai suppliers than from Digi-Key TH.

### 4a. Front-end PCB

| Ref | Part | Value / spec | Notes |
|---|---|---|---|
| J7 | Box header 2×19, shrouded | 2.54 mm, keyed, latching | Signal cable to Nucleo CN10. **18 signals** now |
| J8 | Power connector 2-pin | keyed, latching (JST-XH, Micro-Fit 3.0) | **3.3 V buck output** + GND to Nucleo CN6-4/6 |
| J14 | Solar input 2-pin | keyed, latching, ≥5 A | **24 V bank in** from the charge controller |
| J1–J6 | Pluggable screw terminal, 3-pin | Phoenix MC 1,5/3-ST-3,5 or clone | **One per probe** — six of them |
| J9–J11 | Pluggable screw terminal, 4-pin | same family | **One per SHT45 branch.** `V / SDA / SCL / G`. Silkscreen the destination: `CH0 หัว`, `CH1 ท้าย`, `CH2 นอก` |
| J12 | Pluggable screw terminal, 4-pin | same family | **S88 head.** `5V / GND / A / B` |
| J13 | Pin header 1×3 | 2.54 mm | **Debug UART.** `TX / RX / GND` for a USB-serial adapter |
| **Rail gate** ||||
| Q1 | P-MOSFET SOT-23 | **AO3401A** (AOS) or equiv: R_DS(on) at **V_GS = −2.5 V** ≤85 mΩ, ≥0.5 A | High-side gate for the 3.3 V sensor rail. See *On the P-FET* |
| R1 | Resistor 0805 | 100 kΩ | Gate→source pull-up — **holds the rail off at reset** |
| R2 | Resistor 0805 | 100 Ω–1 kΩ | Gate series, inrush limit |
| **1-Wire probes** ||||
| R3–R8 | Resistor 0805 | **4.7 kΩ** | Six DQ pull-ups, **on `VSENS`**. Sized for the 5–10 m probe runs |
| R9–R14 | Resistor 0805 | 100 Ω | Six DQ series |
| D1–D6 | TVS bidirectional, **low-C signal-line** | Bourns **CDSOD323-T05LC** (1 pF) or onsemi **ESD9B5.0ST5G** (15 pF) | One per DQ. `V_RWM` ≥ 3.6 V — see *Choosing the TVS* |
| **I2C / SHT45 branches** ||||
| U3 | **TCA9548A** | I2C `0x70`, 8-ch bus switch | A2/A1/A0 to GND. Only ch0–ch2 used. **Nothing else is on this bus now** |
| R23 | Resistor 0805 | 10 kΩ | U3 `RESET` pull-up to `VSENS`. **Not optional** — active-LOW, no internal pull-up |
| R15, R16 | Resistor 0805 | 4.7 kΩ | **Upstream** SDA/SCL pull-ups. Board-local, 30 mm — 4.7 kΩ is right here |
| R17–R22 | Resistor 0805 | **2.2 kΩ** | **Downstream**, one pair per channel. **2.2 kΩ, not 4.7 kΩ** — each pair drives 5 m of cable |
| C8 | Ceramic | 100 nF | U3 decoupling |
| **CO2 link** ||||
| U4 | RS-485 transceiver, **3.3 V, true-failsafe** | TI **THVD1450DR** (SOIC-8), or **MAX3485ESA+** / **SP3485EN-L** | `DE` and `!RE` tied together to `S88_DE`. See *The link* for why not an auto-direction part |
| C9 | Ceramic | 100 nF | U4 decoupling |
| D7, D8 | TVS bidirectional | ≥ ±12 V standoff, low-C | On `A` and `B` at the board end |
| **Power — input protection (§5)** ||||
| Q2 | P-MOSFET, ≥60 V | **DMP6023LE-13**, SOT-223 | Reverse polarity on the 24 V input. **Drain (and tab) to J14, source to the load** — see *Q2 — the reverse-polarity FET is wired backwards on purpose*. **Not** a 30 V part |
| R38 | Resistor 0805 | 470 kΩ | Q2 gate → GND. Turns the FET on **and** limits D10's current — the two are a pair |
| D10 | Zener, 250 mW | **12 V** (BZX84C12 SOT-23 / MMSZ5242B SOD-123) | Q2 gate → source, cathode at source. **Not optional** — without it V_GS reaches −32 V against a ±20 V limit |
| D9 | TVS unidirectional | **SMBJ33A** (33 V standoff, 600 W) | Across the 24 V input, **after Q2** — that is what lets it be unidirectional |
| F1 | Fuse, **time-lag (T)**, 5×20 mm cartridge preferred | 2 A, **I²t ≥ 0.5 A²s** | 24 V input. Sized by I²t, not amps: hot-plug inrush through Q2's body diode is ~0.1 A²s and will nuisance-blow a low-I²t 1206 |
| C11 | Electrolytic / polymer | 100 µF, **≥63 V** | 24 V bulk, before the bucks. **63 V, not 50 V** — D9 clamps as high as 53.3 V |
| **Power — the two bucks (§5)** ||||
| U6, U7 | Synchronous buck converter | **TI LM5164DDAR**, 8-pin SO PowerPAD | **Same part twice.** 100 V V_in, 1 A, 10.5 µA I_q. U6 → 5.1 V ungated (S88); U7 → 3.3 V (MCU + `VSENS`). Solder the EP |
| L1, L2 | Inductor, shielded | **47 µH**, I_sat **≥1.5 A**, DCR ≤300 mΩ | One value for both rails. Sat rating is set by the chip's 1.5 A limit, not by the load |
| C12, C13 / C19, C20 | Ceramic X7R | 2.2 µF, **100 V** | `C_IN`, two per buck, **directly across pins 2 and 1** |
| C14 / C21 | Ceramic X7R | 22 µF, 16 V | `C_OUT` |
| C15 | Polymer / electrolytic | 100 µF, 16 V | **U6 only** — bulk for the S88's 300 mA steps down 5 m of cable |
| C16 / C22 | Ceramic **X7R, 50 V** | **2.2 nF** | `C_BST`, BST→SW. **Exactly 2.2 nF** — more stresses the internal VCC regulator, less fails to drive the gate |
| R26 / R33 | Resistor 0805, 1 % | **31.6 kΩ** / **20.5 kΩ** | `R_RON`. Sets ≈400 kHz on each rail |
| R27 / R34 | Resistor 0805, 1 % | 499 kΩ | `R_FB1` |
| R28 / R35 | Resistor 0805, 1 % | **154 kΩ** / **287 kΩ** | `R_FB2` — **the only part that sets the output voltage** |
| R29 / R36 | Resistor 0805, 1 % | **470 kΩ** / **330 kΩ** | `R_A`, Type-3 ripple injection |
| C17 / C23 | Ceramic | 1 nF | `C_A`, ripple injection |
| C18 / C24 | Ceramic **C0G/NP0** | 56 pF | `C_B`, FB coupling. C0G — X7R's DC-bias fall-off matters at this value |
| R30, R31 | Resistor 0805, 1 % | 976 kΩ / 100 kΩ | **Shared** EN/UVLO divider, both bucks. Turn-on 16.1 V, turn-off 15.1 V |
| C25 | Ceramic | 100 pF | Optional, across R31. Keeps the EN node quiet |
| R32 / R37 | Resistor 0805 | 100 kΩ | `PGOOD` pull-up to that rail's own output → test pad. **Not** to the MCU |
| — | LED + series resistor | ×2 | **DNP.** Rail indicators for bring-up only — ~0.5 Wh/day if left fitted |
| **Supply telemetry** ||||
| R24 | Resistor 0805, 1 % | 300 kΩ | `VBAT_SENSE` divider high side |
| R25 | Resistor 0805, 1 % | 30 kΩ | Divider low side — 11:1, so 32 V → 2.9 V |
| C10 | Ceramic | 100 nF | Across R25. Makes a low-impedance source for the ADC |
| **Decoupling** ||||
| C1 | Ceramic | 1–10 µF | `VSENS` bulk (see settle time) |
| C2 | Ceramic | 100 nF | `VSENS` decoupling |
| **Mechanical** ||||
| — | 38-way ribbon + 2× IDC 2×19 socket | 2.54 mm, ≤30 cm | **Clip CN10-6 and plug the matching hole** |
| — | IDC key plug | — | The rotation key (§1) |
| — | 1×8 socket + 2-core lead | 2.54 mm | CN6 tap, positions 4/6/7 wired |
| — | M3 nylon standoffs + screws | ×8 | Both boards |
| — | Cable ties / strain relief | ×11 | Within 30 mm of every entry |
| TP1–14 | Test pads | — | `VSENS`, all six DQ, `SENS_GATE`, `V5_S88`, **`24V_PROT`**, `VBAT_SENSE`, `GND`, upstream SDA/SCL |
| TP15–20 | Test pads | — | The three downstream SDA/SCL pairs. Without these, a dead SHT45 and a dead mux channel look identical |
| TP21–24 | Test pads | — | `PGOOD` and `SW` of each buck. `SW` is where you confirm the switching frequency and catch COT bursting |
| TP25, TP26 | Test pads | — | **`24V_RAW`** (J14 side of Q2) and **Q2's gate**. With `24V_PROT` at TP1 these three tell a blown F1, a dead Q2 and a missing gate clamp apart in one measurement |

**Deleted from the previous revision:** U2 (SCD41), C3 (its 10 µF local bulk), C7
(its 100 nF), and U1a/U1b/U1c with C4–C6 — the SHT45s are no longer on this board.

### 4b. Out at the sensors — not on the PCB

| Where | Part | Notes |
|---|---|---|
| Each probe ×6 | **100 nF** | Across that probe's VDD/GND at the **far** end |
| Each SHT45 ×3 | **SHT45-AD1B** + **100 nF** | Cap across VDD/VSS at the sensor. Small vented housing |
| SHT45 #2 only | **Radiation shield** | Louvered/Stevenson type, sensor below, north-facing. Without it the ambient reference reads 10–15 °C high in sun |
| S88 head | **Senseair S88 LP** (004-1-0101) | Ventilated radiation shield, **diffusion area facing down**, PTFE/Gore membrane |
| S88 head | **MAX485** (or 3.3 V equiv) | `DE`+`RE` tied to the S88's `UART_R/T` |
| S88 head | **3.3 V LDO**, 100 mA | Powers the transceiver off the 5 V rail. **Do not** load `DVCC_out` |
| S88 head | 100 µF + 100 nF | Bulk for the 300 mA peaks, close to `G+` |
| S88 head | Reverse-polarity FET + TVS | The S88 is *unprotected against surges and reverse connection* |

---

## 5. Power — 24 V solar

**This replaced the battery entirely in 2026-08.** The node is fed from a **24 V
battery bank behind a charge controller**, not from cells, and that change
propagates further than anything else in this revision: it is what makes the S88
affordable to run continuously, and it is what broke `battery_read_mv()`.

### What the node must survive

| Parameter | Value | Why |
|---|---|---|
| Nominal | 24 V | 2× 12 V lead-acid, or 8S LiFePO4 |
| Normal range | **21–29 V** | discharged → absorb/equalise |
| **Design range** | **18–32 V continuous** | margin both ways |
| **Part rating** | **≥60 V** | a controller fault, a disconnected battery, or a cold-morning panel Voc can put far more than 29 V on the wire |

Fit **F1 (2 A time-lag) → Q2 (reverse-polarity P-FET) → D9 (SMBJ33A TVS) →
C11 (100 µF)** in that **physical order along the trace** from J14 — F1 and Q2 are
series elements, D9 and C11 shunt to ground, and *The `24V_PROT` node* below draws
what that actually means. This is a long DC run into a metal building;
treat it as an outdoor cable, not as a bench supply.

### Q2 — the reverse-polarity FET is wired backwards on purpose

`Q2` was one BOM row and one box in a diagram, and it has two ways of being built
wrong that both *pass a bench test*. This section is its schematic.

**The part.** Diodes Incorporated **DMP6023LE-13**, a P-channel enhancement-mode
MOSFET in **SOT-223** (3 pins + tab). The numbers that matter:

| Parameter | Value | What it means here |
|---|---|---|
| V_DS | **−60 V** | Satisfies §5's ≥60 V rule. It sees the full reverse bank voltage |
| **V_GS max** | **±20 V** | **The number that shapes this whole circuit** — see *Why the Zener is not optional* |
| R_DS(on) | **28 mΩ** @ V_GS = −10 V (35 mΩ @ −4.5 V) | 2.8 mV of drop at our 100 mA peak |
| I_D | 7 A (T_a) / 18.2 A (T_c) | ~70× the peak this node draws |
| V_GS(th) | ≤ **3 V** @ 250 µA | *Not* a logic-level part despite the "LE" — do not gate it from 3.3 V |
| C_iss | ≈ **2.6 nF** | Sets the turn-on ramp with the gate resistor |
| P_D | 2 W (T_a) | Against ~0.3 mW dissipated |
| **Tab / case** | **DRAIN** | Sits at the *raw, unprotected* input. Layout consequence below |

Like Q1, this part is **chosen for its voltage rating, not its current rating**.
The node draws about **5 mA average and 100 mA peak** at 24 V (the S88's 300 mA and
the LoRa TX's 150 mA are both reflected through the bucks, so they arrive here
divided by roughly 24/5 and 24/3.3). Every current axis is over-specified by more
than an order of magnitude.

#### The circuit

```
   J14 (+)
      │
   [ F1 ]   2 A time-lag cartridge
      │
      ●──── 24V_RAW ── TP25        the tab (= drain) sits at THIS potential
      │
      │ D
   ┌──┴───┐
   │  Q2  │  DMP6023LE       body diode:  D ──►|── S   (points at the LOAD)
   └──┬───┘
      │ S                  G ──●── TP26
      │                        │
      ●──── 24V_PROT ── TP1    ├──[ D10  12 V ]──── to S   (cathode at S)
      │                        │
      ├──► D9, C11, U6, U7     └──[ R38  470 k ]─── GND
      │    (all in PARALLEL — see the next section)
      │
   J14 (−) ─────────────────────────────────────── GND
```

Reading it as a netlist, because the drawing above is the part people get wrong:

| Terminal | Net | Note |
|---|---|---|
| **Drain** (and tab) | `24V_RAW` — the J14 side, after F1 | Yes, the drain faces the supply |
| **Source** | `24V_PROT` — the load side, feeding D9/C11/U6/U7 | |
| **Gate** | junction of `R38` (to GND) and `D10`'s anode | |
| `D10` cathode | **Source**, i.e. `24V_PROT` | Zener sits gate-to-source, physically at the FET |

> **Pin numbering:** on Diodes' SOT-223 MOSFETs this is pin 1 = G, pin 2 = D,
> pin 3 = S with the tab tied to D. The **tab = drain** is confirmed by the
> datasheet's case-connection note; **verify the 1/2/3 assignment against the
> datasheet's "Pin Out — Top View" diagram before committing the footprint.**

#### Why the drain faces the supply

A P-channel MOSFET's body diode runs **drain → source**. That single fact decides
the orientation, and it is the opposite of how the same part is wired in an
ideal-diode/ORing circuit — which is exactly why this trips people.

- **Correct polarity.** The body diode is forward-biased the instant power is
  applied, so `24V_PROT` comes up one diode drop below `24V_RAW`. The gate, pulled
  toward GND by `R38`, then goes strongly negative with respect to the source, the
  channel enhances, and the 0.7 V collapses to 2.8 mV. The diode gets the circuit
  started; the channel does the work.
- **Reversed polarity.** `24V_RAW` sits *below* GND. `24V_PROT` is held at GND by
  the load, so the body diode is reverse-biased and blocks. The gate sits at GND
  through `R38` and the source sits at GND, so **V_GS = 0** and the channel is off
  too. No current anywhere, nothing warm, and **F1 does not blow** — a wiring
  mistake costs nothing but the time to notice it.

**Wire it the other way round** — source to the supply, drain to the load — and it
still works perfectly on the bench with correct polarity. Reversed, the body diode
now points from the load node back into the negative supply terminal, so current
runs GND → load → body diode → supply, and the "protection" does nothing except
blow F1 with luck and destroy the bucks without it. **This failure is invisible
until the day it matters**, which is why the reverse test is a numbered bring-up
step and not an optional extra.

#### Why the Zener is not optional

With the gate pulled to GND and the source at the bank voltage,
**V_GS = −V_bank**: −24 V nominal, **−32 V at the top of the design range**, against
an absolute-maximum gate rating of **±20 V**. A bare gate resistor destroys this
FET — not immediately, and not on a 12 V bench supply, but on a 24 V bank on an
absorb cycle.

`D10` clamps V_GS at about −12 V. `R38` is what limits the Zener current, so the
two are a pair; neither works alone:

| Part | Value | Why this value |
|---|---|---|
| `R38` gate → GND | **470 kΩ** | Sets Zener current to 26 µA at 24 V and 43 µA at 32 V — **0.6 mW, or 15 mWh/day** against the node's 2500 mWh/day. 100 kΩ would cost 5× that for nothing; 1 MΩ starts to be noise-sensitive on a long outdoor input |
| `D10` gate → source | **12 V**, 250 mW (BZX84C12, SOT-23; or MMSZ5242B, SOD-123) | Comfortably above the −10 V where R_DS(on) is specified, and comfortably below the ±20 V limit. **Cathode to source, anode to gate** |

> **The Zener runs far below its test current, and that is fine here.** A 12 V
> Zener characterised at 5 mA sits nearer 10–11 V at 26 µA. That still exceeds the
> −10 V at which the 28 mΩ figure is specified, so nothing is lost. It does mean
> you cannot pick a 16 V or 18 V part and assume the soft knee will save you at
> 32 V input — stay at 12 V.

`R38` × `C_iss` = 470 kΩ × 2.6 nF ≈ **1.2 ms**, so the gate ramps rather than
snaps. That is a small bonus for EMI and for the bucks' input step, but read the
inrush trap below before treating it as a soft-start: it is not one.

#### Why the order is F1 → Q2 → D9, and not F1 → D9 → Q2

This is deliberate and it is the reason `D9` can be a **unidirectional** SMBJ33A
rather than a bidirectional part.

Put the TVS **ahead** of the FET and a reversed connection forward-biases it
directly across the bank. An SMBJ conducts hundreds of amps in that direction; F1
opens, if you are lucky, and the TVS is destroyed either way — every time someone
guesses the polarity of two faded wires. Put it **after** the FET, as here, and the
reverse voltage never reaches it: `Q2` blocks first, so the TVS only ever sees
positive transients, which is precisely what a unidirectional part is for.

The cost is that `Q2`'s drain is exposed to the unclamped input for the nanoseconds
before `D9` conducts. That is survivable at these energies because the FET is *on*
when it happens: the surge passes through the channel, `D9` clamps `24V_PROT` at
≤53.3 V, and `Q2`'s own V_DS stays near zero against its −60 V rating. Its V_GS
stays clamped by `D10` throughout.

**What each part is actually for**, since three protection devices in a row invite
the assumption that they overlap:

- **F1** — a *downstream* short: a buck failing shorted, or C11 failing shorted.
  It does nothing for reverse polarity, because with `Q2` correct there is no
  current to blow it.
- **Q2** — reverse polarity, and nothing else.
- **D9** — positive transients: switching on the charge controller, nearby
  lightning, a long DC run acting as an antenna.
- **C11** — bulk, and damping the resonance between the input cable's inductance
  and the bucks' ceramic input capacitors.

> **Q2 is not an ideal diode and does not block reverse *current*.** Once enhanced,
> the channel conducts both ways. If the bank voltage ever falls below `24V_PROT`,
> C11 discharges back into it. That is harmless at these energies and is worth
> stating only so nobody designs on the assumption that it does not happen.

#### Two traps

**1. Hot-plug inrush goes through the body diode, not through the channel — so the
1.2 ms gate ramp is not a soft-start.** Connecting a charged bank to a discharged
board dumps ~2.6 mC into `C11` plus the bucks' 8.8 µF of ceramic input capacitance,
through the body diode, limited only by cable resistance. Estimate ~0.1 A²s of
I²t. **Specify F1 by I²t, not just by amps**: a 5×20 mm **T2A** cartridge has ample
margin; some 1206 SMD "slow-blow" 2 A parts are rated near 0.1 A²s and will
nuisance-blow on connection. §4a offers both footprints — **prefer the cartridge**,
which is also the one you can replace on a pole with a screwdriver. If you increase
`C11`, check the FET's pulsed body-diode rating as well as the fuse.

**2. The tab is the drain, which means it sits at `24V_RAW` — unprotected,
unclamped input.** It is the largest copper feature on the part and the obvious
place to pour a plane for thermal reasons. Do not: the thermal argument is void
here (0.3 mW), and that copper is on the wrong side of both the FET and the TVS.
Keep the drain pour minimal, keep clearance to the GND pour and the enclosure
appropriate for a 60 V part on a long outdoor cable, and put the heat-sinking
effort where it is actually needed, which is nowhere on this board.

### The `24V_PROT` node — D9, C11 and both bucks are all in parallel

**"F1 → Q2 → D9 → C11" describes physical order along the trace, not an electrical
chain**, and the shorthand has caused enough confusion to be worth replacing with a
drawing. Only **F1 and Q2 are series elements**. Everything after Q2 hangs off a
single node:

```
   Q2 source
       │
       ●━━━━━━┳━━━━━━━━┳━━━━━━━━━━━━┳━━━━━━━━━━━━┳━━━━━━━━━━┳━━━  24V_PROT
       │      │        │            │            │          │
      TP1   ══╪══     ═╪═         ══╪══        ══╪══     [R24 300k]
              │        │            │            │          │
            [D9]     [C11]     [C12+C13]     [C19+C20]      ●──── VBAT_SENSE
           SMBJ33A   100 µF     2×2.2 µF      2×2.2 µF      │      → PB3
          cathode ↑   + ↑        100 V         100 V     [R25 30k]
              │        │            │            │          │
              │        │        U6 pin 2     U7 pin 2       │
              │        │         (VIN)        (VIN)         │
       ───────┴────────┴────────────┴────────────┴──────────┴──── GND
              ↑        ↑            ↑            ↑
          at J14   next to it   ≤5 mm from   ≤5 mm from
                                 U6's pins    U7's pins
```

Read as a netlist, since polarity is where this node gets destroyed:

| Part | Terminal | Goes to |
|---|---|---|
| **D9** SMBJ33A | **cathode — the banded end** | `24V_PROT` |
| | anode | `GND`, straight back to J14's ground pin |
| **C11** 100 µF ≥63 V | **`+`** | `24V_PROT` |
| | `−` | `GND` |
| **C12, C13** 2.2 µF/100 V | — | across **U6's** pins 2 (VIN) and 1 (GND) |
| **C19, C20** 2.2 µF/100 V | — | across **U7's** pins 2 (VIN) and 1 (GND) |
| **U6** pin 2 | — | `24V_PROT` |
| **U7** pin 2 | — | `24V_PROT` |
| **R24** 300 kΩ | — | `24V_PROT` (top of the `VBAT_SENSE` divider) |

**Both bucks tap the same node independently**, which is the whole point of §5's
*Two rails, deliberately independent* — "independent" means independent *rails*,
not independent *inputs*. They share this node and the 24 V bank behind it; what
they do not share is any path from one output to the other.

**`VBAT_SENSE` is measured here, not at `24V_RAW`**, and that is deliberate: the
divider then sits inside D9's clamp and behind Q2, so it never sees a raw
transient, and it reads 0 V rather than a negative voltage if someone reverses the
supply. The 2.8 mV of error Q2 contributes is three orders of magnitude below the
divider's own tolerance.

#### Why the physical order still matters, even though it is all one node

Electrically these parts are in parallel; on copper they are not, and the sequence
along the trace is a real specification:

1. **D9 sits closest to J14/Q2.** A surge should be shunted to ground *at the door*
   so its current never flows through the rest of the board's copper. An SMBJ33A
   passes up to **11.3 A** while clamping (600 W ÷ 53.3 V) — put it 40 mm
   downstream of C11 and that current takes a tour of your board on the way.
2. **C11 immediately after it**, so the electrolytic sits inside the clamp rather
   than being the first thing a transient meets.
3. **Each buck's own `C_IN` within 5 mm of its own VIN/GND pins.** C11 cannot do
   this job — see *The bucks themselves*, layout rule 1. The LM5164 datasheet's
   converse rule also applies: bulk is *required* if the part is more than about
   5 cm from the input source, which is what C11 is for.
4. **R24/R25 last**, farthest from both SW nodes. It is a 27 kΩ analog source; it
   has no business near switching copper.

**Trace width is set by the surge, not by the load.** The DC current here is 100 mA
peak, which any width carries — but D9's 11.3 A pulse and its return want a wide
trace, so do not route `24V_PROT` as a thin signal track just because the load is
small. **Give D9's anode and C11's `−` their own wide copper straight back to
J14's ground pin**, and keep that return off the analog ground and off R25. This is
the one node on this board where "ground is just ground" is wrong.

#### Three ways this node gets built wrong

**1. D9 backwards.** The SMBJ33A is **unidirectional**. Banded end (cathode) to
`24V_PROT`, unbanded to GND. Fitted the other way it is a plain forward diode
across your 24 V rail — F1 opens instantly if you are lucky, and the TVS is dead
either way. **Q2 cannot save you**, because D9 is downstream of it.

**2. C11 backwards.** It is polarised: `+` to `24V_PROT`. A reversed 100 µF
electrolytic on a 24 V rail vents. Same note as above — being downstream of Q2, the
reverse-polarity FET offers no protection against a part fitted backwards *on the
board*. Q2 protects against a reversed *supply*, nothing else.

**3. Choosing a low-ESR polymer for C11 because low ESR sounds better.** It is not
better here. The input cable's inductance (~5 µH for a 5 m run) resonates with the
bucks' 8.8 µF of ceramic input capacitance at roughly **24 kHz**, and connecting a
charged bank rings that circuit. Cable resistance alone (~0.2 Ω for 5 m of 18 AWG,
round trip) already puts the damping ratio near 0.5, and D9 backstops whatever is
left — so this is margin, not a crisis. But **C11's job here is damping, not ripple
current** (the local ceramics carry the 400 kHz ripple), so its ESR is a feature.
Prefer the **aluminium electrolytic** over the polymer, and read the "≥63 V"
requirement as covering both D9's 53.3 V clamp *and* this ring.

### Two rails, deliberately independent

```
  24 V ──[F1]──[Q2 rev]──[D9 TVS]──┬──[U6 buck]── 5.1 V ── J12 ══ 5 m ══> S88
                                   │              UNGATED, always on
                                   │
                                   ├──[U7 buck]── 3.3 V ──┬── J8 ─> Nucleo CN6-4
                                   │                      │
                                   │                      └──[Q1 gate]── VSENS
                                   │                          (probes, SHT45s, mux)
                                   └──[R24/R25]── VBAT_SENSE ─> PB3 (ADC1_IN2)
```

**Why two bucks and not a cascade.** A 24→5 V stage feeding a 5→3.3 V stage would
put the radio downstream of the CO2 sensor's 300 mA peaks. Independent rails mean
an S88 fault — or a shorted 5 m cable — cannot brown out the LoRa transmitter or
the MCU. Each is sized for its own load: **U6 for 300 mA peak / 18 mA average**,
**U7 for the ~150 mA LoRa TX peak**.

> **U7's output must never exceed 3.6 V.** CN6-4 is rated **3.0–3.6 V** (UM2592
> Table 9) and the STM32WL's absolute maximum VDD is 3.6 V. Use a fixed 3.3 V part,
> not an adjustable one with a trim pot somebody can turn.

### The bucks themselves — U6 and U7

Until this revision "U6 buck / U7 buck" was a box in a diagram and one BOM row, which
is not enough to build from. This section is the schematic.

**They are the same chip, twice.** `sourcing-th.md` names it: **TI LM5164DDAR**, a
monolithic synchronous step-down (buck) converter in an 8-pin SO PowerPAD package.
Not a module, not a breakout board, and not an LDO — a 24→3.3 V linear regulator
would burn 86 % of the input power as heat. Both power MOSFETs are **inside the
chip**; what you add outside is **one inductor, five capacitors and five resistors
per rail**. The only differences between U6 and U7 are the feedback divider and two
timing parts.

**Why this part and not a cheaper one.** Two reasons, and neither is negotiable:

1. **100 V input rating.** §5's rule is ≥60 V because a controller fault or a
   cold-morning panel `Voc` can put far more than 29 V on the wire. The popular
   28 V modules (TPS54202, MP1584, and every LM2596 breakout on Lazada) have no
   margin at all against a 24 V bank that reaches 28.8 V on absorb.
2. **10.5 µA sleep quiescent current**, and this one is easy to underrate. U7's
   *average* load is about 1 mA at 3.3 V — **3.3 mW**. A perfectly ordinary buck
   with 200 µA of quiescent current burns 200 µA × 24 V = **4.8 mW** doing nothing,
   so the regulator would consume more than the entire node it powers. The LM5164
   costs 250 µW. Both parts together are **12 mWh/day** against the 2500 mWh/day
   budget.

> **Do not substitute an adjustable module with a trim pot.** §5 already says this
> about U7's 3.6 V ceiling; the LM5164 sets its output with two fixed 1 % resistors,
> which is the form that warning assumes. A pot that can be knocked to 4 V kills
> CN6-4 and the STM32WL.

#### Pin by pin, and what each one connects to

DDA package, top view, pin 1 at the dot. Both rails wire identically except where
the table says otherwise.

| Pin | Name | Connect to |
|---|---|---|
| 1 | `GND` | Ground plane. Adjacent to VIN, which is what makes the input loop small — see *Layout* |
| 2 | `VIN` | `24V_PROT` (the node after F1/Q2/D9). **`C_IN` goes directly across pins 2 and 1** |
| 3 | `EN/UVLO` | The **shared** `R30/R31` divider — both bucks tie here. Sets a 16.1 V turn-on |
| 4 | `RON` | `R_RON` to GND. Sets the on-time, and therefore the switching frequency |
| 5 | `FB` | Divider tap `R_FB1`/`R_FB2`, **plus** the `C_B` ripple-injection cap |
| 6 | `PGOOD` | Open drain. 100 kΩ pull-up to that rail's own output, then a **test pad**. **Not to the MCU** — see below |
| 7 | `BST` | `C_BST` 2.2 nF 50 V X7R to `SW`, and nothing else. The bootstrap diode is internal |
| 8 | `SW` | The inductor, and `C_BST`'s other end. This is the only high-dV/dt net on the board |
| EP | pad | **No internal connection, but solder it** to the GND pin and a copper pour with vias. θ_JA is 43.4 °C/W only if you do |

**There is no `VCC` pin and no `VCC` capacitor.** The internal bias regulator needs
no external cap for stability — if you are used to other TI wide-V_IN parts and go
looking for that pin, it is not missing, it does not exist here.

**`PGOOD` deliberately does not reach the MCU.** §2's connector contract is
**eighteen** signals and adding a nineteenth breaks forward compatibility with every
front-end board again. A power-good line the firmware cannot act on anyway — if 3.3 V
is gone the MCU is not running to read it — is not worth that. Pull it up, bring it
to a test pad, and leave the LED footprint **DNP**: an always-lit indicator on each
rail is ~0.5 Wh/day, a fifth of the node's entire budget, spent telling nobody in a
sealed box on a pole that the power is on.

#### The power stage

```
                        ┌─────────────┐
   24V_PROT ─────┬──────┤2 VIN   SW 8 ├──────┬───[ L 47 µH ]───┬──── V_OUT
                 │      │             │      │                 │
             [ C_IN ]   │  LM5164DDAR │      └──[C_BST 2.2nF]──│──┐
             2× 2.2 µF  │   U6 / U7   │                        │  │
               100 V    │             │                     [ C_OUT ]
                 │      │      BST 7 ├────────────────────────│──┘
                GND     │             │                        │
                        │1 GND    EP  │                       GND
                        └──┬───────┬──┘
                          GND    GND plane
```

`EN/UVLO`, `RON`, `FB` and `PGOOD` are left off this drawing on purpose — the first
three are drawn below, and the fourth goes nowhere.

#### The feedback and ripple network — the part that is easy to get wrong

A constant-on-time converter has **no error amplifier and no compensation network**;
the FB pin drives a *comparator*. That is why the LM5164 needs no loop compensation,
and it is also why it needs something the datasheets of ordinary current-mode parts
never mention: **at least 20 mV of ripple at FB, in phase with the inductor current.**
Give it less and it does not merely regulate poorly — it bursts, firing several
on-times in quick succession and then idling, which looks exactly like a defective
chip on a scope.

With ceramic output capacitors there is almost no ESR, so almost no in-phase ripple
appears at `V_OUT` to divide down to FB. The datasheet's Type-1 fix is to add a
series resistor to `C_OUT`, deliberately making the output ripple *worse* — on a rail
feeding an ADC reference and a 923 MHz radio, that is the wrong trade. **This design
uses Type-3 ripple injection** (datasheet Table 6-1): an `R_A`/`C_A` network across
the switch node manufactures a triangular ramp, and `C_B` couples it into FB. The
output stays quiet and the comparator still gets its ramp.

```
   V_OUT ──┬──[ R_FB1 499k ]──┬── FB (pin 5)
           │                  │
           │            [ R_FB2 ]           R_FB2 = 1.2 / (V_OUT − 1.2) × R_FB1
           │                  │
           │                 GND
           │
           └──[ C_A 1 nF ]──┬──[ R_A ]──── SW (pin 8)
                            │
                            └──[ C_B 56 pF ]──── FB (pin 5)

   RON (pin 4) ──[ R_RON ]── GND        R_RON(kΩ) = V_OUT × 2500 / F_SW(kHz)

   24V_PROT ──[ R30 976k ]──┬──[ R31 100k ]── GND
                            │
                            └──── EN/UVLO of BOTH U6 and U7
```

**One UVLO divider serves both bucks.** `EN/UVLO` is a high-impedance input, so the
same tap drives both pins — two fewer resistors and 22 µA instead of 44 µA. It also
means the two rails come up and go down together, which is what we want: there is no
sequencing requirement between them, and no reason to let one live while the other
does not.

#### Component values

Both rails run at **≈400 kHz**. Lower would need a bigger inductor; higher buys
nothing here, and the minimum 50 ns on-time is nowhere near binding at these
voltages (at 32 V in and 3.3 V out the on-time is still 256 ns).

| | **U6 → 5.1 V** (S88) | **U7 → 3.3 V** (MCU + `VSENS`) |
|---|---|---|
| `R_RON` | **31.6 kΩ** 1 % | **20.5 kΩ** 1 % |
| → F_SW | 403 kHz | 401 kHz |
| `R_FB1` | **499 kΩ** 1 % | **499 kΩ** 1 % |
| `R_FB2` | **154 kΩ** 1 % | **287 kΩ** 1 % |
| → V_OUT typ | **5.088 V** | **3.286 V** |
| → V_OUT worst case | 4.96 – 5.21 V | **3.20 – 3.37 V** |
| `L` | **47 µH**, I_sat ≥ 1.5 A, shielded | same part |
| → ΔI_L at 24 V | 212 mA | 151 mA |
| → I_L peak at max load | 406 mA | 225 mA |
| `C_IN` | **2× 2.2 µF / 100 V X7R** | same |
| `C_OUT` | **22 µF / 16 V X7R** + **100 µF** polymer | **22 µF / 16 V X7R** |
| `C_BST` | **2.2 nF / 50 V X7R** | same |
| `R_A` | **470 kΩ** | **330 kΩ** |
| `C_A` | **1 nF** | **1 nF** |
| `C_B` | **56 pF C0G/NP0** | **56 pF C0G/NP0** |
| → FB ripple, 18 / 24 / 32 V in | 19 / 21 / 23 mV | 20 / 21 / 22 mV |

Where the numbers come from, in datasheet equation order: `R_RON` from Eq. 12,
`R_FB2` from Eq. 10 (V_REF = 1.2 V, `R_FB1` kept inside TI's recommended
100 kΩ–1 MΩ), `L` from Eq. 20, `C_OUT` from Eq. 21, `C_A` from Eq. 24, `R_A` from
Eq. 25 and `C_B` from Eq. 26 for a 75 µs settling target. The FB-ripple row is the
one to check if you change anything: it must stay **above 20 mV at 24 V and above
12 mV at 18 V**, and it is what breaks first when somebody "improves" the output
capacitor.

**Why one 47 µH part for both rails.** U7 alone would want 47 µH and U6 alone 33 µH,
but 47 µH on U6 only makes its ripple current smaller, and one part number is worth
more than 30 mA of ripple. The saturation requirement is set by the chip, not the
load: peak current limit is **1.5 A typical**, so the inductor must not saturate
below that or a fault turns into a runaway instead of a foldback.

**The extra 100 µF on U6.** `C_OUT` for ripple is 22 µF; the polymer bulk is for the
S88's 300 mA current steps arriving down 5 m of cable, whose inductance the ceramic
alone cannot cover. The head also carries its own 100 µF (§4b) — both are needed, at
opposite ends.

#### Layout — four rules, in the order they will bite you

1. **`C_IN` across pins 2 and 1, within 5 mm.** The input loop carries a square wave
   of the full output current with nanosecond edges; its area is the dominant EMI
   source on this board. TI's pinout puts VIN and GND adjacent precisely so one
   0805 can close that loop.
2. **Keep the `SW` copper small.** It is the only net that slews 24 V in a few
   nanoseconds. Make it just big enough for the inductor pad and `C_BST`, and never
   run `FB`, `RON`, the DQ lines or `VBAT_SENSE` under it.
3. **`RON` needs less than 20 pF to ground.** Resistor right at pin 4, short trace,
   no plane poured under it. This one is not intuitive — a noisy RON pin shifts the
   on-time and the converter jitters.
4. **Both bucks in one corner, far from J1–J12.** Six bit-banged open-drain 1-Wire
   lines with microsecond bit slots, three unshielded 5 m I2C branches, a 27 kΩ ADC
   divider and a 923 MHz radio all live on this board. Give the switchers their own
   corner with the 24 V input, and let the sensor front-end have the other.

#### Three traps specific to these two parts

**1. `C11` needs to be 63 V, not 50 V.** D9 is an SMBJ33A, whose *clamping* voltage
during a surge is up to **53.3 V** — above a 50 V electrolytic's rating. The bulk cap
sits on the clamped side of the TVS, so it sees that. The BOM row now says ≥63 V.

**2. The 5.1 V rail is short-circuit-safe, and that is the whole argument for two
bucks.** A crushed or waterlogged 5 m cable to the S88 head trips U6's peak limit at
1.5 A, folds the valley limit back to 1.2 A, and — if it gets hot enough — thermal
shutdown with automatic recovery. §5 claims "an S88 fault cannot brown out the LoRa
transmitter"; this is the mechanism behind that claim, and it only holds because U7
is fed from 24 V independently rather than from U6's output.

**3. Efficiency at 18 mA is not the 90 % in §5's energy table.** At that load the part
is in diode-emulation pulse-skipping, where efficiency is 80–85 %. The table's
0.24 Wh/day of buck loss should read closer to 0.4 Wh/day — still noise against
80 Wh/day of panel yield, and still the right architecture, but do not quote 90 % at
a design review.

### 5 V goes down the cable, not 24 V

At 300 mA over 5 m of 24 AWG (0.84 Ω round trip) the drop is **250 mV**, landing
4.85 V at the S88 — above its 4.5 V minimum with margin. Regulate U6 to **5.1 V** to
absorb it, or use 22 AWG (0.5 Ω, 150 mV) if you want more.

The alternative — sending 24 V and bucking at the head — is worse despite the
lower current. It puts a switching converter, an inductor and a bulk electrolytic
**inside the humid greenhouse enclosure**: more parts to corrode, more heat next to
a sensor whose accuracy you care about, and switching EMI at the measurement point.
For a 5 m run it buys nothing.

### The energy budget, and why it stopped being interesting

| Load | Draw | Per day |
|---|---|---|
| **S88 LP, continuous** | 18 mA @ 5 V = 90 mW | **2.16 Wh** |
| Buck losses (~90 % eff.) | | ~0.24 Wh |
| MCU + LoRa + probes + SHT45s | ~1 mA avg @ 3.3 V | <0.1 Wh |
| ST-LINK, **if** you power it | ~5 mA @ 3.3 V | ~0.4 Wh |
| **Total** | | **≈2.5 Wh/day** |

Against ~80 Wh/day from a 20 W panel at four peak-sun hours, the continuously
powered CO2 sensor is **~3 %** of the yield. Overnight carry is 12 h × 90 mW ≈
**1.1 Wh**; three cloudy days is ~7.5 Wh. Size the bank for the cloudy-day case, not
for the node.

**Consequences of that table**, all of which simplify the design:

- CO2 duty-cycling is **deleted** — see *The CO2 sensor* in §3.
- The AO3401A's 5 µA I_DSS leakage budget is **no longer a constraint**. Keep the
  gate for fault recovery, not for energy. *On the P-FET* below is retained because
  R_DS(on) and the 300 mA question still matter, but read its leakage arithmetic as
  history.
- Stop2 sleep current no longer needs to be single-digit µA to be acceptable.

### Three traps

**1. `battery_read_mv()` no longer works, and the fix is a divider.** VREFINT
measures VDDA — which is now U7's regulated 3.3 V output, a number that is the same
whether the bank is full or nearly flat. §2 predicted this: *"add a regulator
between battery and MCU and it starts reporting the regulator output instead."*
The replacement is **R24/R25 (300 kΩ / 30 kΩ, 11:1) → PB3 (ADC1_IN2)**, giving
2.9 V at a 32 V bank. C10 across R25 makes the source low-impedance at the sampling
instant; 300 k‖30 k is 27 kΩ, which needs a long ADC sampling time.

> **VREFINT is still needed — for a different job.** Read it to recover the *actual*
> VDDA, then scale the divider reading against that. Otherwise a 3 % buck tolerance
> becomes 3 % of error on every bank-voltage report. The firmware keeps VREFINT and
> gains a second channel; it does not drop the first.

**2. The ST-LINK is unpowered, so the VCP is dead.** Feeding 3V3 directly at CN6-4
leaves the debug section unpowered — UM2592: *"the programming and debugging
features are not available, since the ST-LINK is not powered."* On a battery that
was the right trade. On solar you could now afford to power it (~0.4 Wh/day), but
that needs the 5V/E5V jumper set correctly, so this design instead brings
**USART1 (PB6/PB7) out to J13** and you attach a USB-serial adapter. Always
available, no jumper, no ST-LINK. Snap the ST-LINK section off or leave it dark.

**3. The charge controller's low-voltage disconnect can cut you off.** Most
controllers have a LOAD terminal that disconnects around 11.5 V/cell to protect the
bank. A node on that terminal goes silent exactly when you most want telemetry
about a failing solar system. **Connect to the battery terminal**, with F1/Q2/D9 as
your own protection — and accept that the node is then responsible for not flattening
the bank, which at 2.5 Wh/day it will not.

---

## 6. Cables and enclosures

**The box is no longer one box.** There is a dry enclosure outside the greenhouse
holding both PCBs, and **four sensor locations** on cables:

| # | Destination | Cable | Contents |
|--:|---|---|---|
| 1–6 | Water probes `P0`–`P5` | 5–10 m each | DQ twisted with GND, VDD separate |
| 7 | SHT45 #0 — **หัวโรงเรือน** | ≤5 m | `V / SDA / SCL / G` |
| 8 | SHT45 #1 — **ท้ายโรงเรือน** | ≤5 m | `V / SDA / SCL / G` |
| 9 | SHT45 #2 — **นอกโรงเรือน** | ≤5 m | `V / SDA / SCL / G` |
| 10 | S88 LP — **กลางโรงเรือน** | 5 m | `5V / GND / A / B` |
| 11 | Solar input | — | 24 V + GND from the charge controller |

**Eleven cable entries.** That, not the PCBs, now sets the enclosure size — the same
lesson this document already learned once when the probe count went to six.

- **Probes:** 3-pin pluggable screw terminals, six of them. Field re-termination
  with cold hands is the design case; JST crimps will not survive it. Silkscreen
  `V / DQ / G` on every one, and **number them `P0`–`P5` to match `DS_PROBE_BUSES`
  and the dashboard metric names** (`temp_hot`, `temp_cold`, `temp_p2`…`temp_p5`).
- **Probe cable length: 5–10 m.** Every value in §3 — pull-up, TVS budget, `VSENS`
  bulk, settle time — is sized for it. Past 20 m the DQ pull-up drops to 2.2 kΩ; past
  ~30 m you need a DS2483 line driver rather than a bigger resistor.
- **Probe cable:** one twisted pair = **DQ + GND**, VDD on a separate conductor.
  Cat5 is ideal. Prefer **plain UTP over shielded** — shielded roughly doubles
  capacitance per metre, the one cable property these values depend on.
- **SHT45 branches: ≤5 m, and this is a hard number.** At 5 m with 2.2 kΩ the I2C
  rise time is 0.60 µs against a 1000 ns budget. At 10 m it fails. If a run must be
  longer, fit a P82B715 on that branch (§3) — do not simply lengthen the cable.
  Twist **SDA with GND**; run SCL in the second pair. One Cat5 per branch.
- **Label both ends of every branch.** Three identical connectors, three identical
  cables, three identical sensors. A swapped J10/J11 silently exchanges "inside the
  greenhouse" for "ambient reference" and the data still looks plausible.
- **Sensor housings — each location needs its own.** Small, vented, sensor facing
  down. The SHT45s tolerate 100 %RH and recover from condensation, so they need
  protection from *liquid* water and sun, not hermetic sealing.
- **SHT45 #2 needs a radiation shield, not an enclosure.** Louvered or
  Stevenson-type, sensor below the shield, north-facing. An unshielded sensor in
  Thai sun reads **10–15 °C above true air temperature** and the ambient reference
  becomes worse than useless — it becomes misleading.
- **The S88 head is the demanding one.** Ventilated radiation shield, **diffusion
  area facing downward** so condensate drips away rather than pooling on the optical
  window, PTFE/Gore membrane, never sealed. See *The deployment risk* in §3 — this
  sensor is rated 0–85 %RH non-condensing and the greenhouse will test that.
- **The front-end enclosure no longer needs venting.** Nothing in it measures air
  any more. It should be **IP65 and as sealed as the glands allow** — a change from
  the previous revision, where the SCD41 forced a vent. Keep the desiccant habit
  anyway if the box is opened often in humid weather.
- **RF:** keep the antenna clear of metal, and keep the ribbon away from the RF
  section and the CN12 SMA. A 30 cm ribbon draped over the antenna is a real
  antenna-detuning problem — and there are now eleven cables competing for the same
  space, so plan the routing rather than discovering it.
- **Test points:** the twenty in the BOM. With six probes, three I2C branches and a
  Modbus link, "which one?" is now the first question of every field failure.

---

## 7. Bring-up order

Do this before the boards go in a sealed box on a pole. The
[`bluepill_f103c8_dump`](../README.md) diagnostic is the right first power-on test
and ports to the WL55 in a few lines.

**Power first, and on its own** — this is new, and it is the step that protects
everything else:

1. **Bring up the 24 V front end with no PCB loads.** F1 → Q2 → D9 → C11, then
   confirm **U6 = 5.1 V** and **U7 = 3.3 V ±2 %** on a bench supply swept
   **18 → 32 V**. U7 must never read above **3.6 V** at any input voltage; if it
   does, stop — CN6-4 and the STM32WL are both absolute-max 3.6 V.
   **Then scope the bucks before trusting the DMM**, at TP21–24:
   - **`SW` must switch at ≈400 kHz** with clean, evenly spaced pulses under load.
     Bursts — several on-times crowded together, then a long gap — mean the FB
     ripple is under 20 mV. That is an `R_A`/`C_A`/`C_B` problem, not a bad chip;
     see *The bucks themselves*.
   - **At no load, pulse-skipping is correct, not a fault.** Diode emulation is what
     buys the 10.5 µA quiescent current.
   - **Sweep 18 → 32 V while watching `SW`.** COT feeds V_IN forward, so the
     frequency should barely move. If it walks, suspect a noisy `RON` node.
   - **`PGOOD` high on both rails** before anything else is connected.
2. **Deliberately short the 5.1 V output at J12.** U6 should fold back and survive;
   **U7 and the 3.3 V rail must not move at all.** This is the one test that proves
   §5's "an S88 fault cannot brown out the LoRa transmitter" — do it once, on the
   bench, with the boards not yet on a pole.
3. **Read the input chain at TP25 / TP1 / TP26, all referenced to the `GND` pad,
   before anything else.** Three DMM readings at 24.0 V in, correct polarity,
   separate every way this chain gets built wrong:

   | TP25 `24V_RAW` | TP1 `24V_PROT` | TP26 gate | Diagnosis |
   |---|---|---|---|
   | 24 V | ≈24 V | **≈12 V** | **Correct.** V_GS = −12 V, channel fully enhanced |
   | **0 V** | 0 V | 0 V | **F1 open**, or no supply reaching J14 |
   | 24 V | **≈23.3 V** | ≈ same as TP1 | **`R38` open/unfitted, or `D10` shorted** → V_GS ≈ 0, running on the body diode |
   | 24 V | ≈24 V | **≈0 V** | **`D10` missing, backwards or wrong value** → V_GS = −24 V, past the ±20 V limit. Replace D10 **and** Q2 |
   | 24 V | **0 V** | 0 V | Q2 open — dead, or not actually soldered down |

   **Row 3 is a 0 V-versus-0.7 V test, not a precision measurement.** At the 5 mA
   average load the real drop across Q2 is 0.14 mV and no field meter resolves it;
   what you are looking for is whether it is *0.7 V*, which means the channel never
   enhanced and the whole node is being fed through the body diode. That survives a
   bench test and then runs hot and sags in the field.

   **What this measurement cannot catch: Q2 fitted backwards** (source and drain
   swapped). Every reading above comes out normal. Only step 4 finds it.
4. **Reverse the bench supply deliberately.** Q2 should block: **zero current**,
   `24V_PROT` at 0 V, and nothing warm anywhere. If current flows, Q2 is in
   backwards — source and drain swapped — which is the one build error that passes
   every correct-polarity test. Do this now, on a bench, rather than discovering it
   at a charge controller with the polarity guessed from faded insulation.
5. **Only then connect J8 to the Nucleo.**

**Cable alone, nothing connected** — five minutes that pays for itself:

6. **Buzz the signal ribbon end to end** against §2's map. Confirm continuity on
   the **eighteen** contract positions and, critically, that **nothing** rings out
   to CN10-2, -4, -7, -22, -32 or -38. Note that **-35 and -37 are now used**
   (`DBG_TX`/`DBG_RX`) — they left the forbidden list this revision. A ribbon
   assembled one position out is the single most likely fault in this design, and
   this is the last moment it is cheap.
7. **Check the key.** With CN10-6 clipped and the socket hole plugged, the socket
   must seat one way and refuse the other. If it seats both ways, the key is not
   done — go back and do it.

**Front-end alone, ribbon unplugged from the Nucleo** — this is why the test pads
exist:

8. Pull `SENS_GATE` high → `VSENS` = 0 V. Pull it low → `VSENS` = 3.3 V, and all
   eight switched signal lines (six DQ, upstream SDA, SCL) idle high through their
   pull-ups. **`V5_S88` must be 5.1 V in both states** — it is not gated.
9. **Pin-probe every DQ line** → `pull-up=1 pull-down=1` on all six (proves each
   pull-up is really on `VSENS`, not GND). Steps 6 and 7 catch the
   resistor-to-GND class of fault — all-zero scratchpads that pass CRC and decode
   as a convincing `0.00 °C`, which `ds18b20_read()` rejects explicitly.
10. **Check `VBAT_SENSE` before trusting any reported voltage.** With 24.0 V at J14,
   PB3 should sit at **24.0 / 11 = 2.18 V**. Sweep the supply and confirm it tracks
   linearly. A divider off by one resistor decade reports a plausible-looking
   constant and you will believe it for months.

**Connected:**

11. Gate off → with the rail off, SDA and SCL must read **0 V**, not a diode drop
   below 3.3 V — anything else means an I2C pull-up landed on permanent 3V3
   instead of `VSENS`.
12. **Plug in ONE probe at a time**, starting at `P0`, and confirm the reading
    appears under the expected metric name before adding the next. Warm each probe
    by hand and watch the right series move. Six probes plugged in at once, with one
    channel miswired, is a much worse debugging session than six one-probe checks.
13. All six fitted → six plausible, **independent** temperatures, CRC OK. Two
    channels that track each other exactly are two connectors wired to one MCU pin.
14. **I2C scan with every mux channel closed** → exactly **`0x70` and nothing
    else**. No `0x44` (an SHT45 wired upstream, which will collide the moment a
    channel opens) and no `0x62` — the SCD41 is gone; if something answers there,
    you are looking at the wrong board.
15. **Scan again with each channel open in turn** → exactly one `0x44` each time.
    Two at once means more than one channel is open; none means that channel's
    pull-ups or its sensor are missing. Do this before trying to read anything: an
    address that does not appear is a wiring or pull-up fault, while an address that
    appears but returns CRC failures is a signal-integrity one, and the two want
    different fixes.
16. **Read all three SHT45s with the real 5 m cables fitted, not bench leads.**
    This is the step that catches a 4.7 kΩ downstream pull-up: it works on a 300 mm
    lead and starts NACKing at 5 m. Then **breathe on one sensor** — exactly one
    series should move. If two move, two channels are bridged; if the wrong one
    moves, the channel-to-location mapping is off and every reading is mislabelled
    from then on. Verify against the silkscreen: `CH0 หัว`, `CH1 ท้าย`, `CH2 นอก`.
17. **Modbus link, before the S88 is fitted.** Loop `A`/`B` at the far end of the
    5 m cable and confirm the MCU receives its own transmission. That separates
    "the RS-485 path works" from "the sensor answers", which are different faults
    with different fixes.
18. **Read the S88** (once fitted — see the staged plan in §3). Outdoor air is
    **~420 ppm**; a greenhouse in daylight runs lower as the crop draws it down, and
    higher at night. A CRC failure on the Modbus frame is a wiring or termination
    problem; a valid frame with an implausible number is a sensor or calibration
    problem.
19. **Wiggle test.** With everything reading, flex the ribbon and tug each of the
    eleven cable entries. Nothing should glitch. This is the test that finds a
    marginal IDC crimp before the pole does.
20. Full cycle → wake, read, TX, sleep. Confirm the gateway logs `6/6 probes and
    3/3 air sensors reading`, and confirm the sensor rail actually goes off
    **during** the sleep rather than at the next wake — while `V5_S88` stays up.

---

## References

- **UM2592** — STM32WL Nucleo-64 board (MB1389) user manual: the authority for
  this board. **Table 18** ("Pin assignment of the ST morpho connectors") is the
  source for §2's CN10 map and Appendix A's CN7 map; **Table 17** ("ARDUINO
  connectors pinout") for the CN6 power tap; **Table 9** ("External power sources:
  3V3") for the CN6-4 battery input; §6.6.5 and the solder-bridge tables cover the
  VCP/D0-D1 arrangement.
- TI **LM5164** datasheet (SNVSAU4) — U6/U7. Pin functions (Table 4-1), the 1.2 V
  FB reference and Eq. 10, the `R_RON` frequency Eq. 12, inductor Eq. 20, output cap
  Eq. 21, and **Table 6-1 "Ripple Generation Methods"** — the Type-3 network this
  design uses and the 20 mV minimum FB ripple that makes it necessary.
- TI **AN-1481** — *Controlling Output Ripple and Achieving ESR Independence in
  Constant On-Time (COT) Regulator Designs*. The long form of why §5's bucks need a
  ripple-injection network at all.
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
