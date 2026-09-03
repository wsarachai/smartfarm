# water-temp-node — hardware interface spec

The real (non-breadboard) build is **two PCBs joined by a cable**:

| Board | What it is | Carries |
|---|---|---|
| **Brain** | NUCLEO-WL55JC1 (off-the-shelf) | STM32WL55JC, radio, RF switch, TCXO, antenna, ST-LINK |
| **Front-end** | Large custom PCB, this spec | **6× DS18B20 probe connectors**, **4× I2C branch connectors** behind a bus switch (3× SHT45 + the SCD41 CO2 head), AO3401A rail gate, pull-ups, line protection, **24 V solar input and one off-the-shelf buck module** |

The front-end is a **large board sitting beside the Nucleo**, not stacked on it,
and the two are joined by a short cable. This document specifies that joint: the
connectors, the signals crossing them, and the front-end circuit behind them.

Everything here is a contract the **firmware already assumes**. Where a value is
forced by code, the code is cited — change one and you change the other.

- Firmware pin map: [`include/node_config.h`](../include/node_config.h)
- Gate + sleep handling: [`src/main.cpp`](../src/main.cpp)
- Wire frame (probe → metric): [`src/lora/lora_packet.h`](../src/lora/lora_packet.h)
- Cable-length guidance: [README → *Probe cable runs*](../README.md)
- Shopping list with quantities and purchase waves: [`parts-list-th.md`](parts-list-th.md)
- Manufacturer part numbers and substitution rules: [`sourcing-th.md`](sourcing-th.md)
- Thai translation: [`hardware-interface.th.md`](hardware-interface.th.md) — **this English file is the source of truth**; update it first

> **Revision 2.0 (2026-09) — the CO2 sensor is a Sensirion SCD41 on I2C, and the
> RS-485 subsystem is gone with the Senseair S88 that needed it.** The S88
> wanted 4.5–5.25 V and Modbus over a 5 m differential pair, which cost this
> board a second regulator, two transceivers, an LDO at the head, and three of
> the eighteen signals crossing the joint. The SCD41 runs from the same 3.3 V as
> everything else, speaks I2C with a CRC-8 on every word, and takes on-demand
> single shots — so it joins the bus switch as a fourth branch and goes back
> behind `SENS_GATE` with the rest of the front-end. The contract drops to
> **fifteen** signals, the 5 V rail and its module are deleted, and the node's
> whole budget falls to **~0.8 Wh/day**.
>
> The deployment is a **mushroom house (โรงเรือนเพาะเห็ด)**, which is why the
> part changed: dark, 90–95 %RH, misted, 15–30 °C, and CO2-rich by design. Text
> written for a plant greenhouse — sun loads, radiation shields, crops drawing
> CO2 down in daylight — has been corrected throughout, not just in this section.
>
> The S88 revision is preserved in
> [`hardware-interface-s88.md`](hardware-interface-s88.md); it stays readable
> because the **S88 GH** is the fallback if the CO2 range requirement ever
> widens past the SCD41's specified 400–5000 ppm. Do not build from it.

> **Revision 1.0 (2026-09) — the discrete bucks are gone.** U6/U7 were two TI
> LM5164 constant-on-time converters with a hand-calculated Type-3 ripple-injection
> network, ~30 passives and a shared UVLO divider. They are replaced by two
> **off-the-shelf Traco TSR 1 modules** (5 V and 3.3 V, SIP-3, three pins each).
> The full LM5164 design is preserved unchanged in
> [`hardware-interface-back.md`](hardware-interface-back.md) — read it if you ever
> want the µA-class quiescent current or the 100 V input rating back; do **not**
> build from it. §5 *The buck modules* records what changed and what was given up.

---

## 1. The joint — which bus, and why

Six probe connectors, four remote-sensor branch connectors, protection, a buck
module and a 24 V solar input do not fit on a
68.6 × 53.4 mm ARDUINO outline. Once the front-end has to be its own
large board it becomes a cabled peripheral rather than anything that mounts on
the Nucleo — which is what decides the connector question.

### The recommendation

**Two cables, deliberately separate:**

| Cable | Carries | Nucleo end | Front-end end |
|---|---|---|---|
| **Signal** | `SENS_GATE`, 6× `DQ_Pn`, `SDA`, `SCL`, `DBG_TX/RX`, `VBAT_SENSE`, 2× `GND` | **2×19 IDC socket over the whole of morpho CN10** | shrouded, keyed 2×19 box header |
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
   │  U3 mux -> J9..J12 (3x SHT45 + SCD41, 5 m) │
   │  U7 TSR 1-2433 · J14 24 V solar in         │
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

**Fifteen** signals. Freeze this table; it is the interface. Every other position
on CN10 is **left unconnected** at the front-end.

Note what is *not* here: **four** remote sensors — three SHT45s and the SCD41 —
add **no** signals at all, even though they sit at four different places in the
mushroom house. They are separated on the front-end by a bus switch that is
itself an I2C device (§3), so the joint does not grow with them. That is the
single most valuable property of this interface: sensor count is decoupled from
connector width.

> **The contract went twelve → eighteen → fifteen. Read the direction.** The
> 2026-08 revision *grew* it: a Senseair S88 replaced the SCD41 and spoke
> UART/Modbus rather than I2C, bringing `S88_TX`/`S88_RX`/`S88_DE`; a 24 V solar
> supply put the MCU behind a buck and broke `battery_read_mv()`'s VREFINT
> trick, bringing `VBAT_SENSE`; and an unpowered ST-LINK forced the debug log
> onto its own pins, `DBG_TX`/`DBG_RX`.
>
> Revision 2.0 (2026-09) *shrinks* it: the SCD41 returns, so all three `S88_*`
> signals go. **`PC1`, `PC0` and `PA7` are now free and are deliberately left
> unused** — the front-end must not connect them. `DQ_P4` stays on **PB8** even
> though `PC1` is available again; moving it back would churn this table, the
> firmware pin map and the board layout for no gain.
>
> **Older front-end boards are not forward-compatible at any of these steps** —
> and an eighteen-signal board is not merely over-provisioned for this revision,
> it is wrong: it has a 5 V rail this design does not generate.

| Signal | WL55 pin | Connector | Direction | Electrical | Forced by |
|--------|----------|-----------|-----------|------------|-----------|
| `V3V3_MCU` | — | **CN6-4** (`3V3`) | in to brain | **3.3 V regulated** from the 24 V buck, ≤250 mA peak | §5 — *no longer the battery* |
| `GND` (power) | — | **CN6-6/7** | — | the supply return, its own conductor | — |
| `SENS_GATE` | **PA8** | **CN10-16** | out of brain, push-pull | active **LOW** = rail on; Hi-Z = off via 100 k | `node_config.h` `DS_PWR_*` |
| `DQ_P0` | **PA5** | **CN10-11** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h` `DS_PROBE_BUSES` |
| `DQ_P1` | **PA4** | **CN10-17** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P2` | **PA9** | **CN10-19** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P3` | **PC2** | **CN10-21** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `DQ_P4` | **PB8** | **CN10-27** | bidir, open-drain | 1-Wire — moved off PC1 in 2026-08 and **stays here** | `DS_PROBE_BUSES` |
| `DQ_P5` | **PB10** | **CN10-25** | bidir, open-drain | 1-Wire | `DS_PROBE_BUSES` |
| `I2C_SDA` | **PA11** | **CN10-5** | bidir, open-drain | I2C @ 100 kHz; parked **analog** in sleep | `node_config.h` `I2C_SDA_PIN` |
| `I2C_SCL` | **PA12** | **CN10-3** | out of brain, open-drain | I2C @ 100 kHz | `node_config.h` `I2C_SCL_PIN` |
| `VBAT_SENSE` | **PB3** | **CN10-31** | in to brain, **analog** | divided 24 V bank voltage, 0–3.0 V — ADC1_IN2 | `battery.cpp` |
| `DBG_TX` | **PB6** | **CN10-35** | out of brain, push-pull | USART1 TX, 115200 → 3-pin debug header | `DEBUG_UART_*` |
| `DBG_RX` | **PB7** | **CN10-37** | in to brain | USART1 RX ← debug header | `DEBUG_UART_*` |
| `GND` (signal) | — | **CN10-9**, **CN10-20** | — | one net (GND); wire **both** — see *Returns vs guards* | — |
| `VSENS` | — | — | front-end internal | **gated** 3.3 V to probes, **all four** I2C branches, mux **and** all pull-ups | `main.cpp` `gate_on()` |

**The front-end generates exactly one internal rail: `VSENS`.** If you meet a
second one in an older drawing or an older board, that board is not this design —
see the note above.

**Sizing `V3V3_MCU`:** the largest single draw is still the **LoRa TX's ~150 mA**,
but the SCD41 is now on this rail too and bursts to **175 mA typ / 205 mA max**
while it measures. Those two never overlap — the radio is powered only after the
gate has closed (§7, and `main.cpp`) — so the rail is sized for ~205 mA, not for
their sum. The six probes add ~9 mA during their conversion.

> **`VBAT_SENSE` is the one analog conductor in the ribbon.** It sits at CN10-31,
> between LED3 (CN10-30, left open) and AGND (CN10-32, left open) — the quietest
> neighbourhood on the connector, which is why it goes there and not on a spare
> next to a bit-banged 1-Wire line. Filter it at the front-end (see §5) and sample
> it while the probes are idle.

`VSENS` is generated **on the front-end** and never crosses back to the Nucleo. The
only power crossing the joint is `V3V3_MCU` and its ground, on their own cable.
The buck module, and the 24 V input that feeds it, live entirely on the front-end.

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
| 13  | PA6       | spare                  | 14 | PC0     | spare — **freed in rev 2.0**, leave open |
| 15  | PA7       | spare — **freed in rev 2.0**, leave open | 16 | **PA8** | ✅ `SENS_GATE` |
| 17  | **PA4**   | ✅ `DQ_P1`            | 18 | NC     | guard (GND at front-end only) |
| 19  | **PA9**   | ✅ `DQ_P2`            | 20 | **GND** | ✅ GND return 2 of 2 |
| 21  | **PC2**   | ✅ `DQ_P3`            | 22 | PB0     | ⛔ VDD_TCXO |
| 23  | PC1       | spare — **freed in rev 2.0**, leave open | 24 | NC   | guard (GND at front-end only) |
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
| C1                          | 1–10 µF    | 1     | On the PCB — `VSENS` bulk (bounded by `DS_POWER_SETTLE_MS`, § below) |
| *(no ref — not on the PCB)* | **100 µF + 100 nF** | 1 ea | **At the SCD41 head**, for its 205 mA bursts — the reservoir that keeps them off 5 m of cable |

So **twelve 100 nF parts: ten out at the sensors, two on the board** — and the
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

### The I2C sensors — three SHT45s and the SCD41, all remote

**Four branches, one per channel.** Every sensor on this bus is at the far end of
its own cable; nothing measures anything inside the enclosure:

| Sensor | Addr | Mux ch | Location | Frame slot |
|---|---|---|---|---|
| SHT45 #0 | 0x44 | 0 | **หัวโรงเรือน** — house head | `air_temp0` / `humidity0` |
| SHT45 #1 | 0x44 | 1 | **ท้ายโรงเรือน** — house tail | `air_temp1` / `humidity1` |
| SHT45 #2 | 0x44 | 2 | **นอกโรงเรือน** — outside, ambient reference | `air_temp2` / `humidity2` |
| **SCD41** | **0x62** | **3** | **กลางโรงเรือน** — mid-house, **at crop level** | `co2` |

The MCU box lives **outside** the mushroom house, because the house is humid and
misted and the electronics should not be. Only sensors go inside. Every branch is
**≤5 m**.

**The SCD41 gets a channel even though its address does not collide.** 0x62 and
0x44 could coexist upstream of the switch, and an earlier revision did exactly
that — back when the SCD41 was *on the board*. It cannot now: an unswitched
branch puts its ~320 pF on the bus permanently. With the 4.7 kΩ upstream pull-ups
that is a ~2 µs rise time against standard mode's 1 µs limit, and worse again
whenever an SHT45 channel is open at the same time. Load-stacking is the whole
reason the switch is here; the CO2 sensor does not get an exemption from it.

Sensor #0 keeps mux channel 0 because its history is continuous with the old
single-SHT45 series — see *Which sensor is which* below. **Sensor #2 is the
ambient reference**, and it is the only one that sees sun: it needs a
**radiation shield**, not merely an enclosure, because an unshielded sensor in
Thai sun reads 10–15 °C above true air temperature and the reference becomes
worthless. The three inside the house need no shield — a mushroom house is dark.

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

The CO2 sensor is **back on this bus** as of revision 2.0 — see *The CO2 sensor*
below for why, and for the one way it is not like the other three: it draws
**205 mA in bursts**, which is ten times anything else the front-end powers.

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
house still fit inside the same contract — and a fourth, the SCD41, joined them in
revision 2.0 without widening it by a single signal. **Nothing sits upstream of
the switch:** the SCD41's 0x62 would not collide with 0x44, but it is 5 m away, so
it takes channel 3 rather than loading the bus permanently.

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
| U3 TCA9548A | `0x70` | upstream | — | 8-channel bus switch; A2/A1/A0 to GND. **Channels 0–3 used**, four spare |

**Nothing is upstream of the switch except the switch itself**, which is what makes
the **all-channels-closed I2C scan** in §7 a clean test: with every channel closed
you should see **`0x70` and nothing else**. Open channels 0–2 and each shows
**`0x44`**; open channel 3 and it shows **`0x62`**.

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
          ├─ R22 2k2 ── SC2   │  and that sensor never answers.
          ├─ R39 2k2 ── SD3   │  Channel 3 is the CO2 branch and is no
          ├─ R40 2k2 ── SC3   ┘  exception: same value, same reason.
          ├─ U3.VCC     + C8 100nF to GND     TCA9548A
          └─ J9/J10/J11/J12 pin V             3V3 out to the four branches

  SDA_UP ─┬─ PA11  (CN10-5)             SCL_UP ─┬─ PA12  (CN10-3)
          └─ U3.SDA                             └─ U3.SCL

  SD0    ─┬─ U3.SD0                     SC0    ─┬─ U3.SC0
          └─ J9.SDA                             └─ J9.SCL      -> หัวโรงเรือน
  SD1    ─┬─ U3.SD1                     SC1    ─┬─ U3.SC1
          └─ J10.SDA                            └─ J10.SCL     -> ท้ายโรงเรือน
  SD2    ─┬─ U3.SD2                     SC2    ─┬─ U3.SC2
          └─ J11.SDA                            └─ J11.SCL     -> นอกโรงเรือน
  SD3    ─┬─ U3.SD3                     SC3    ─┬─ U3.SC3
          └─ J12.SDA                            └─ J12.SCL     -> CO2 กลางโรงเรือน

  ACROSS AN SHT45 BRANCH  (4 conductors, <=5 m, 24 AWG; SDA twisted with GND)
     VSENS  ───────────  SHT45 VDD
     SDn    ───────────  SHT45 SDA
     SCn    ───────────  SHT45 SCL
     GND    ───────────  SHT45 VSS

  ACROSS THE CO2 BRANCH  (same 4 conductors, but 22 AWG -- see the 205 mA problem)
     VSENS  ───────────  SCD41 VDD + VDDH      <-- BOTH, tied at the sensor
     SD3    ───────────  SCD41 SDA
     SC3    ───────────  SCD41 SCL
     GND    ───────────  SCD41 GND

  AT EACH SHT45  (far end of the branch)
     SHT45 VDD --[100nF]-- SHT45 VSS      <-- at the SENSOR, not on the PCB

  AT THE CO2 HEAD  (far end of channel 3)
     SCD41 VDD --[100uF]-- SCD41 GND      <-- low-ESR; the burst reservoir
     SCD41 VDD --[100nF]-- SCD41 GND      <-- ceramic, right at the pads
     SCD41 VDD -- VDDH                    <-- short link, AT the sensor
     heater resistor pad ..............   <-- DNP, see "Condensation"

  GND    ─┬─ U3.GND, J9/J10/J11/J12 pin G
          ├─ U3.A0, U3.A1, U3.A2      all three LOW = address 0x70
          └─ C8 low side

  open   ─── U3.SD4..SD7, U3.SC4..SC7   four unused channels, leave unconnected
```

Pin **names** above are what to wire to; pin **numbers** differ between the
TCA9548A's TSSOP-24 and QFN-24 packages, so take those from the datasheet for the
part you actually buy. For reference, the SHT4x DFN-4 is `1 = SDA`, `2 = VSS`,
`3 = VDD`, `4 = SCL` — confirm against your footprint before fabricating, because
all three sensors share it and a mirrored footprint is three mistakes, not one.

#### Five ways this goes wrong

1. **`RESET` left floating.** It is active-LOW with no internal pull-up, so a
   floating pin means the switch may sit in reset and *nothing* downstream ever
   answers. Every I2C device on this node is behind the switch — there is nothing
   upstream left to still work and reassure you — so the symptom is a total I2C
   blackout, including CO2. R23 to `VSENS` fixes it; a direct tie to `VSENS` is
   acceptable if you never want to reset it from firmware.
2. **`A0`/`A1`/`A2` left floating.** The address is then undefined and the switch
   answers at something other than 0x70, or intermittently. Tie all three to GND.
3. **Missing downstream pull-ups.** The single most likely fault on this board.
   The mux is a set of analog switches: it passes SDA and SCL through, but the
   pull-ups do not propagate. A channel wired with sensor but no resistors scans
   as empty, exactly like a dead sensor.
4. **A sensor accidentally wired upstream.** An SHT45 there collides with the
   other two the moment a channel opens; the SCD41 there puts ~320 pF permanently
   on the bus (§ *Why there is a bus switch*). Bring-up step 8 catches both: with
   all channels closed you must see **`0x70` and nothing else** — no `0x44`, and
   no `0x62`.
5. **Any of it on permanent 3V3 instead of `VSENS`.** Same rule as everything else
   on this board — including R17–R22 and R39/R40, which are easy to forget because
   they sit on the far side of the switch.
6. **A branch wired with 4.7 kΩ instead of 2.2 kΩ.** This is the new one, and it is
   nasty because it *mostly* works: 4.7 kΩ over 5 m gives a 1.27 µs rise against a
   1000 ns budget, so the bus passes on the bench with a short lead and starts
   throwing intermittent NACKs once the real cable is fitted. R17–R22 and R39/R40
   are **2.2 kΩ**; R15/R16 upstream stay 4.7 kΩ because they only drive 30 mm of
   trace.


> **Which sensor is which is decided by the channel, not by the part.** Sensor
> index = mux channel = frame slot = dashboard metric name, wired together by
> `I2C_MUX_CHANNELS` in `node_config.h`. Three identical 0x44 parts are otherwise
> indistinguishable, so **silkscreen the channel number next to each one**.

**The old placement rules are gone, because the sensors left the board.** There is
no longer any question of an SHT45 sitting downwind of the CO2 sensor or over the
gate FET's copper — nothing on this PCB measures air any more. What replaces them:

1. **Each sensor needs its own small vented housing at its own location**, and #2
   needs a **radiation shield** (§6). The front-end enclosure itself no longer has
   to be vented for humidity — but see *The CO2 sensor*, because the SCD41's head
   emphatically does.
2. **Silkscreen the destination, not just the channel number.** `CH0 → หัว`,
   `CH1 → ท้าย`, `CH2 → นอก` next to J9/J10/J11. Three identical connectors feeding
   three identical parts on three identical cables is exactly the situation where a
   swapped pair goes unnoticed for a season — the data still looks plausible, it is
   just attributed to the wrong end of the greenhouse.
3. **Label both ends of every branch cable.** The failure this prevents is not
   electrical; it is someone reconnecting J10 and J11 after maintenance and
   silently swapping "inside the greenhouse" with "ambient reference".

### The CO2 sensor — Sensirion SCD41, on I2C

**Revision 2.0 (2026-09).** The CO2 part is a **Sensirion SCD41** (`SCD41-D-R2`,
Digi-Key TH ฿673.73), photoacoustic NDIR, I2C at **0x62**, on **mux channel 3**
and a 5 m cable to the middle of the house at crop level. Everything below is
from the SCD4x datasheet **v1.7, April 2025**.

It replaces a **Senseair S88 LP**, and the reason is the building. The S88 LP is
rated **0–50 °C, 0–85 %RH** with the RH limit derating to 45 % at 50 °C. A
mushroom house sits at **90–95 %RH**, is misted directly, and condenses. That was
never going to hold.

| | S88 LP (replaced) | **SCD41** |
|---|---|---|
| Supply | 4.5–5.25 V | **2.4–5.5 V** — the same 3.3 V as everything else |
| Interface | UART/Modbus over RS-485 | **I2C, CRC-8 on every 16-bit word** |
| Operating range | 0–50 °C, 0–85 %RH (derating) | **−10–60 °C, 0–95 %RH** non-condensing |
| Duty | continuous — ABC needs it | **on-demand single shot** |
| Average current | 18 mA, forever | **0.45 mA** at one shot / 5 min |
| Peak current | 300 mA | 175 typ / **205 max mA** |
| Output range | 400–10 000 ppm | **0–40 000 ppm** (specified 400–5000) |
| Accuracy | ±40 ppm ±3 % | ±(50 ppm + 2.5 %) 400–1000 · ±(50 + 3 %) to 2000 · ±(40 + 5 %) to 5000 |
| Response τ63 | <40 s | 60 s |
| On the board it costs | 2nd regulator, U4, U5, head LDO, A/B TVS, 3 signals | **one mux channel** |

What that table is really saying: **this sensor costs the board a connector.** It
needs no regulator of its own, no transceiver at either end, and no signal across
the joint — it is the fourth branch on a bus switch that was already there.

> **I2C here loses no data integrity.** The usual objection is that a corrupted
> I2C transaction hands back a plausible wrong number, where a framed protocol
> would fail its checksum and be retried. That does not apply to a Sensirion part:
> **every 16-bit word carries its own CRC-8** (poly 0x31, init 0xFF), which
> `src/sensirion_i2c.cpp` already verifies for the SHT45s. A bad read never
> reaches the cache in `main.cpp`.

#### Where it goes, and why there

**กลางโรงเรือน, at crop level.** CO2 is heavier than air and the blocks are what
produce it, so it stratifies and is densest at bed height. The number that should
drive a ventilation fan is the one the crop is actually sitting in — not the one
at the ridge, and not the one by the door.

That is a different place from all three SHT45s (head, tail, outside), which is
why the SCD41 gets its own cable and its own channel rather than sharing a head.

#### What this node measures, and what it does not

**The design case is ventilation control during fruiting: 400–3000 ppm.** That is
the band where the SCD41 is most accurate — ±(50 ppm + 2.5 %) below 1000 ppm — and
it is the band where the decision actually lives, because oyster mushrooms
answer high CO2 with long stems and small caps.

**During spawn run the house is deliberately CO2-rich and will read past 5000 ppm.**
The SCD41 keeps outputting up to **40 000 ppm**, so the data does not clip — but
above 5000 ppm it is outside its specified accuracy. Treat those numbers as
qualitative ("still colonising"), not as research-grade measurements. If a
future crop plan needs calibrated spawn-run figures, that is the point at which
the **S88 GH** (0–20 000 ppm, 0–95 %RH) in
[`hardware-interface-s88.md`](hardware-interface-s88.md) comes back — and it
brings the 5 V rail and RS-485 back with it.

#### Why it sits behind the gate

An NDIR CO2 sensor usually wants continuous power, for four reasons. Check each
against this one, because if any of them held, the board would need an always-on
rail and `SENS_GATE` would have a hole in it:

| Usual reason to leave a CO2 sensor powered | Does it bind here? |
|---|---|
| Warm-up after power-up | **No** — handled per wake by a **throwaway first shot**, below |
| A filter that needs measurement history | **No** — τ63 is 60 s and the node samples every 15 min |
| Self-heating keeps the optics above the dew point | **Not available** at 1.5 mW — see *Condensation* below |
| A self-calibration period measured in days | **No** — **ASC is switched off here**, see *ASC* below |

So the CO2 sensor sits on `VSENS` with the probes and the SHT45s, and this
board has **no always-on sensor rail at all**. `SENS_GATE` recovers a latched CO2
branch exactly as it recovers a latched SHT45 branch.

**The cost is the throwaway shot**, and Sensirion names this mode and this cost
explicitly. Cutting the supply between measurements is *power-cycled single shot
operation*, and the rule for it is unambiguous: *"Because the sensor requires one
single shot measurement to stabilize after power cycling, the CO2 reading of the
initial single shot after startup should always be discarded."* So
`scd41_read_single_shot(warmup=1)` takes two and discards the first — **~10 s of
sensor-on time per wake**, not optional on a node that power-cycles the sensor 96
times a day.

> **This one has a genuine documentation conflict behind it, and the next person
> to read the datasheet will find the opposite of what this section says.** Get
> it from here rather than rediscovering it:
>
> | Document | Says |
> |---|---|
> | Datasheet **v1.3–v1.6** | *"After a power cycle, the initial single shot reading should be discarded to maximize accuracy."* Plus a second, separate note saying to discard the first reading after `wake_up` |
> | Datasheet **v1.7** (Apr 2025) | **Both statements removed.** The revision history is explicit: *"removed recommendation to discard initial single shot measurement after power cycle (Section 3.11)"* |
> | App note *SCD4x Low Power Operation* v1.0 (Jul 2022) §2.4 | Still says it, and more strongly — *"the CO2 reading of the initial single shot after startup should always be discarded"* |
>
> Sensirion does not say **why** it was removed, and a deletion from a revision
> history is not an affirmative statement that the first reading is now good.
> Note also that the app note is demonstrably stale on another point — its
> flowcharts still show the 1000 ms power-up that the datasheet corrected to
> 30 ms in v1.4 — so it cannot simply be treated as the surviving authority
> either.
>
> **This build keeps the throwaway shot**, and the reasoning is asymmetric rather
> than bibliographic:
>
> - **If it is unnecessary, it costs 0.007 Wh/day** — on a node spending 0.8 of an
>   available 80. Nothing.
> - **If it is necessary and skipped, the sensor reads low.** That is the same
>   failure the ASC section is about: the controller believes the air is fine, the
>   house is under-ventilated, and **nothing on the dashboard looks wrong.**
>
> Pay 0.007 Wh/day for that. Revisit only if Sensirion states positively that the
> first shot is valid — not merely if a future datasheet is silent again.

The app note is on firmer ground about the mode itself, and it confirms the
choice: power-cycled single shot beats leaving the sensor idle between shots
**once the sampling period is above 380 s**, and this node wakes every **900 s**. Below that threshold the sensor's 200 µA idle
current is cheaper than paying for a stabilisation shot every time; above it, the
gate wins. This design is comfortably on the right side of the line — but it is a
line, and a future decision to sample every 5 minutes would cross back over it.

Energy, from the app note's own figure for this mode (Equation 2: **154 mC per
useful single shot at 3.3 V**, which already includes the discarded one):

`154 mC × 96 wakes/day × 3.3 V = 48.8 J ≈` **0.0136 Wh/day**

against a node budget of ~0.8 Wh/day and a panel yield of ~80 Wh/day. It is the
smallest line in §5's table by an order of magnitude.

#### ASC — turn it off, and understand why before you turn it back on

**The SCD41 ships with ASC (Automatic Self-Calibration) enabled** and a baseline
target of **400 ppm**. ASC assumes the sensor is *"exposed to outdoor fresh air at
400 ppm CO2 at least once for >3 minutes after every week of operation"*
(datasheet §3.8) and slowly calibrates the lowest reading it has seen toward that
target.

**A mushroom house never reaches 400 ppm.** A well-ventilated fruiting room sits
at 800–1500 ppm. If the true weekly minimum is 700 ppm and ASC believes it is 400,
ASC drags every reading down by ~300 ppm, and keeps going each period until it
saturates.

Follow that through to the failure: the sensor reads low → the controller
believes the air is fine → **the house is under-ventilated** → long stems, small
caps. That is precisely the outcome the sensor was fitted to prevent, and
**nothing on the dashboard looks wrong while it happens.** This is the single
most dangerous default in this design.

And there is a second, blunter reason that does not depend on the mushroom house
at all: **ASC is not available in power-cycled single shot operation at all.**
The datasheet is unambiguous — *"for power-cycled single shot operation, ASC
functionality is not available in either case"* (§3.11, and it means either
cutting VDD or using `power_down`/`wake_up`). Its bookkeeping counts readings
across a history that does not survive the rail going away. Leaving ASC enabled
here would not give a badly-calibrated sensor so much as an undefined one.

That makes the ASC-off decision doubly determined: wrong for this building, and
unavailable in this mode. It is still written explicitly rather than left to the
sensor, because `persist_settings` state is what a *replacement* sensor arrives
with, and a replacement arrives with ASC on.

So:

- **ASC off.** `set_automatic_self_calibration_enabled` (0x2416) with 0, then
  `persist_settings` (0x3615, 800 ms) to survive the power cycling.
- **The firmware enforces it on every wake**, not once — `scd41_ensure_asc()`
  reads first (1 ms) and writes only on a mismatch, so a replaced sensor is
  corrected automatically and the EEPROM is never written twice for the same
  value. Unlike everything else on this board, this is a setting the firmware
  must actively *change*, because the factory default is the wrong one.
- **Recalibrate by hand with FRC**, in outdoor air, at each crop changeover —
  which is when the house is emptied and cleaned anyway, so it is an existing
  operational rhythm rather than an invented chore.

**The FRC procedure**, and its preconditions are strict (datasheet §3.8.1):

1. Take the head **outdoors**, into open air away from people and exhausts.
2. Let it run in stable, homogeneous CO2 first. The datasheet says >3 minutes;
   the app note (§5.1) is more specific for the mode this node actually uses, and
   its instruction is the one to follow: **run up for 5 minutes at a 1-minute
   sampling period** — five single shots, one minute apart — *"the same procedure
   also applies to performing FRC if the sensor is to be operated in power-cycled
   single shot mode in the field."* FRC on a sensor that has not been measuring
   returns `0xFFFF` and does nothing.
3. Altitude compensation must already be set (below). Supply must be the normal
   3.3 V, not a bench 5 V.
4. `perform_forced_recalibration` (0x362f, 400 ms) with the reference value —
   **~420 ppm** for outdoor air, `SCD41_FRC_TARGET_PPM` in `node_config.h`.
5. Read back the correction it applied. A large jump is worth writing down.

`scd41_perform_frc()` implements this but **does not enforce the preconditions** —
it is a bench/service call, deliberately not wired into the wake loop.

> **Do not "fix" ASC by raising the target instead.** Setting
> `set_automatic_self_calibration_target` to the house's typical minimum looks
> tempting and is a trap: that minimum moves with crop stage, flush timing and
> the ventilation schedule — it is the quantity you are trying to measure.

#### Altitude, not pressure

The SCD41 reads high or low with air density, and it has no barometer. Two
commands can tell it where it is:

- `set_ambient_pressure` (0xe000) — lives in **RAM**, and this node drops the
  sensor's rail every 15 minutes, so it would be lost every wake. Useless here.
- `set_sensor_altitude` (0x2427) — **EEPROM-backed** after `persist_settings`.
  Written once, survives the power cycling. This is the one to use.

`SCD41_SITE_ALTITUDE_M` in `node_config.h` holds it (**330 m** for Maejo), and
`scd41_ensure_altitude()` applies the same read-before-write rule as ASC. Set it
before the first FRC — a recalibration performed at the wrong altitude bakes the
error in.

#### The 205 mA problem, and the three things done about it

This is the one axis on which the SCD41 is *harder* than the part it replaced,
relative to everything else on the rail. It draws **175 mA typ / 205 mA max** in
bursts while measuring — ten times its own average, and ten times anything else
the front-end powers. The datasheet also asks that the supply, measured without
the sensor's own load, *"not vary by more than 30 mV (e.g. ripples or drops caused
by other loads)"*, and recommends giving it a dedicated LDO.

Three mitigations, and together they are enough:

1. **22 AWG on the CO2 branch**, not the 24 AWG used for the SHT45s. Round-trip
   resistance drops to **0.53 Ω** (22 AWG is 52.96 Ω/km, and 5 m out and back is
   10 m), so the worst-case IR drop at 205 mA is **109 mV**: the head sees ~3.12 V
   against a 2.4 V minimum. (24 AWG would give 173 mV — it would still work, but
   there is no reason to spend the margin.)
2. **100 µF + 100 nF at the head**, across VDD/GND and close to the sensor. This
   is the local reservoir that keeps the burst off the cable in the first place,
   and it is the practical substitute for the dedicated LDO the datasheet asks for.
3. **Read it last, inside the gated window.** `main.cpp` addresses the SCD41 only
   after every DS18B20 conversion has been read out and all three SHT45s are done.
   Nothing that cares about a quiet 3.3 V is still working when the bursts start.
   The ordering pays for the power-up wait too: by then the rail has been up for
   the whole 750 ms conversion, so `SCD41_POWER_UP_MS` costs nothing.

> **`VDD` and `VDDH` must be tied together** at the sensor, and the datasheet is
> explicit that both are supplied from the same rail — *"VDD and VDDH must be
> connected to each other close to the sensor on the customer PCB"* (§2.3). On a
> breakout board this is already done; on a bare LGA it is a mistake that is easy
> to make once, and the 5 m of cable is emphatically not "close to the sensor".

#### The 30 mV question — a switcher against an LDO requirement

The clause quoted above deserves its own answer rather than a hand-wave, because
**taken at the regulator this board fails it**:

| | |
|---|---|
| SCD41 asks for (§2.3, unloaded) | **≤ 30 mV p-p**, and *"operating the sensor with a separate LDO is recommended"* |
| U7 delivers (TSR 1, 24 V input models, 20 MHz BW) | **75 mV p-p typ** at **400–600 kHz** (500 kHz typ) |

There is no LDO on this board and there is not going to be one — §5 spent a
section explaining why a second regulator is not worth resurrecting. So the
requirement has to be met somewhere else, and it is: **at the sensor, which is
the only place the datasheet actually specifies.**

**The 5 m cable is the filter.** This is the pleasing part of the design, and it
is worth writing down because it inverts the obvious reading — the long branch
looks like the liability that forces 22 AWG, and it is *simultaneously* the thing
that makes a switching regulator acceptable to a part that asked for an LDO:

- 5 m of twisted pair has a loop inductance of roughly **0.7 µH/m → ~3.5 µH**.
  (This is an estimate from geometry, not a datasheet figure — which is exactly
  why bring-up measures it rather than trusting it.)
- Against the head's **100 µF**, that is a low-pass with a corner near
  **8 kHz** — nearly two decades below U7's switching frequency.
- At 500 kHz the cable's series impedance is inductive, ωL ≈ **11 Ω**, while the
  head capacitance presents its ESR — a few hundred mΩ for a low-ESR 100 µF,
  shunted further by the 100 nF ceramic's 3.2 Ω. The divider is therefore on the
  order of **1:40**, and U7's 75 mV p-p arrives at the SCD41 as **~2 mV**.
- Margin is over an order of magnitude, so the conclusion survives being wrong
  about the inductance by 2× or the ESR by 5×.

**It does not ring.** The obvious objection to putting an LC between a regulator
and a pulsed load is that switching the branch on excites it. The wire's own
**0.53 Ω** damps it: `Q = (1/R)·√(L/C) = (1/0.53)·√(3.5 µH / 100 µF) ≈ 0.35`,
comfortably below 1. Note that 22 AWG was chosen in the section above for its
*resistance* being low, and here the same resistance is wanted for damping — 20
AWG would still give Q ≈ 0.5, so the choice is not delicate in either direction.

> **The 100 µF at the head does two jobs, not one.** It is the reservoir for the
> 205 mA burst *and* the shunt leg of the ripple filter. That is the reason it is
> specified as **low-ESR** in §4b rather than "100 µF, any type": a high-ESR part
> degrades both jobs at once.

Because the inductance figure is an estimate, **bring-up measures this rather
than assuming it** — see §7, where the head's VDD is scoped with the branch
powered and the sensor idle. Under 30 mV p-p passes; anything near it means the
cable is not what this section assumed.

#### The head

The SCD41 is a **10.1 × 10.1 × 6.5 mm LGA, MSL 1, reflow-solderable** — there is
no through-hole variant. Two honest ways to build the head:

| | Part | Notes |
|---|---|---|
| **Breakout** | Sensirion **`SEK-SCD41-SENSOR`** (Digi-Key TH) | VDD/VDDH already bridged, pull-ups fitted, 2.54 mm pins. **The right choice for a research build** — no reflow, no rework risk on a part that costs ฿674 |
| Bare | **`SCD41-D-R2`** on a small custom head PCB | Cheaper per unit and smaller; needs reflow or hot air, and the land pattern in datasheet §4.2 |

Either way the head carries the sensor, the 100 µF + 100 nF, the branch's 2.2 kΩ
pull-up pair, and the **DNP heater pad** below.

##### Building a bare LGA head

Skip this if you buy the breakout — which is the recommendation. It is here so
that the second option is a real option and not a shrug.

**Pinout** (datasheet §2.3, Table 6; top view, 21 pads on a 10.1 × 10.1 mm body):

| Pad | Net | Pad | Net |
|---|---|---|---|
| 1–5 | DNC | 11–18 | DNC |
| **6** | **GND** | **19** | **VDDH** |
| **7** | **VDD** | **20** | **GND** |
| 8 | DNC | **21** | **GND** — four centre pads, all pad 21 |
| **9** | **SCL** | | |
| **10** | **SDA** | | |

Three things about that table are easy to get wrong, and all three are silent
failures:

1. **Pad 8 is DNC, sitting between VDD and SCL.** The bottom row is
   `GND · VDD · DNC · SCL · SDA`, not four signals in a row. A footprint drawn
   from memory puts SCL and SDA one pad to the left and the sensor never answers.
2. **"Do not connect" does not mean "leave unlanded".** The datasheet requires
   that DNC *"pads must be soldered to a floating pad on the customer PCB"* —
   they are mechanical, and the part is held on by them. Draw all 21 pads.
3. **Pin 1 is marked twice**: a circular mark, and a **notched corner on the
   protective membrane**. Use both; the package is very nearly square.

**Pull-ups** live at the head, not on the front-end PCB — that is the whole point
of putting R39/R40 downstream. The datasheet's example value is 10 kΩ for a short
bus; this branch is 5 m and uses **2.2 kΩ** (§ *Wiring it*). Firmware **must only
ever drive SDA and SCL low**, never high — the STM32WL's I2C peripheral is
open-drain and already complies, but a bit-banged fallback written in a hurry is
where this rule gets broken.

**Handling and reflow** — the part is more fragile to process than to use:

- **MSL 1** per IPC/JEDEC J-STD-033B1. Floor time out of bag is unlimited under
  normal factory conditions (≤30 °C / 85 %RH); process within a year of delivery.
  This is the one piece of good news — no bake, no dry cabinet.
- **Reflow to J-STD-020, peak ≤ 245 °C for < 30 s**, ramp-up < 3 °C/s, above
  T_L = 220 °C for < 60 s, ramp-down < 4 °C/s while above T_L.
- **245 °C must not be exceeded anywhere in the part, not just at the pad.** The
  datasheet warns that the cap interior runs hotter than a thermocouple on the
  land reads. If you profile at the pad, leave headroom.
- **Not compatible with vapour-phase reflow.** Hot air or IR/convection only.
- **The white dust cover must not be removed, wetted, or tampered with** — before,
  during, or after. It is not packaging.
- **No extra flux, no second reflow pass, and no board wash afterwards.** A head
  PCB that gets cleaned after soldering is a scrapped sensor.
- **Respect the keep-free area** around the thermal relief hole in the land
  pattern (§4.2 of the datasheet). Solder wicking into it is the classic defect.
- **Reflow shifts the CO2 reading, temporarily.** Full accuracy returns **at most
  five days** after soldering, whether or not the sensor is powered. So: do not
  judge a freshly built head, and **do not run FRC on one** — the datasheet is
  explicit that recalibration should be performed no less than five days after
  assembly. Build the head, then leave it a week before you calibrate anything.

The laser marking on the **sidewall of the cap** carries the variant (SCD40 /
SCD41 / SCD43) and a data-matrix serial. Check it: SCD40 and SCD41 are visually
identical from above, they share the 0x62 address, and the SCD40 **does not
implement `measure_single_shot`** (datasheet §3.11: SCD41 and SCD43 only) — which
is the one command this node depends on. The datasheet does not say how an SCD40
responds to it, so do not plan to detect the mix-up in firmware; read the cap.

**Mounting, for a mushroom house specifically:**

- **No radiation shield.** A mushroom house is dark; there is no solar load to
  shield from. That requirement belonged to the plant-greenhouse revision and has
  been deleted, not merely relaxed.
- **Vented, splash-protected housing** with the **diffusion opening facing
  downward** so condensate drips away instead of pooling, behind a **PTFE/Gore
  membrane** that passes CO2 but not liquid water.
- **Never sealed.** A sealed box measures the CO2 of its own interior, which is a
  number that means nothing.
- **Not in the direct path of a misting nozzle.** This is the one placement rule
  that is specific to this building; site it before you cut the gland.

#### Condensation, and the heater that is not fitted yet

**The SCD41 dissipates ~1.5 mW averaged** (0.45 mA × 3.3 V), which is nowhere near
enough to hold itself above ambient. A sensor that runs a degree or two warm keeps
local humidity off its own dew point for free; this one does not, and it is going
into a house that is misted daily and sits at 90–95 %RH. Be blunt about it: **this
is the one axis on which this design has no passive answer**, and the previous
revision of this board had the protection by accident because its sensor burned
90 mW.

The answer is a **heater resistor pad at the head, laid out now and left DNP**:

- ~**220 Ω across the branch's 3.3 V**, i.e. ~50 mW, ~15 mA — enough to hold a
  small housing a degree or so above ambient.
- It would add **~1.2 Wh/day**, taking the node from ~0.8 to ~2.0 Wh/day — **2.5 %
  of the panel**, and still less than this board drew before the 5 V rail was
  deleted. The energy that deletion freed pays for the heater several times over.
- **Whether it is needed is a question for the logs, not for this document.**
  Deploy with the SHT45s first, log T/RH at mid-house, compute the dew point, and
  fit the resistor if condensation is real. Laying the pad out now is what makes
  that a soldering job rather than an enclosure redesign.

> The heater goes on the **branch's own 3.3 V**, downstream of the gate, so it
> heats only while the node is awake — ~11 s in 900 s. If the logs show
> persistent dawn condensation that duty cycle is not enough, and the honest fix
> is a resistor fed from `V3V3_MCU` ahead of the gate. Do not discover this on a
> pole: decide it from the logs.

#### What the SCD41 reports that this node throws away

`read_measurement` returns CO2 **and** temperature **and** humidity, and
`scd41_read()` hands all three back. Only CO2 is transmitted.

That is deliberate: the SCD41 self-heats, so its own T/RH read high — the
datasheet devotes a whole section (§3.7) to the temperature offset needed to
correct them. An SHT45 is ±0.1 °C / ±1 %RH and unheated. Its T/RH are exposed only
because they arrive for free and are useful in bring-up: **if the SCD41's
temperature reads far above the nearest SHT45, the head is not ventilating.**

### The rail gate — now a recovery mechanism, not an energy measure

**Its purpose changed completely when the supply became solar.** On a battery the
gate existed to stop µA of leakage mattering over months; the 5 µA I_DSS budget
below was a real constraint. On a 24 V bank fed by a panel, that arithmetic is
irrelevant — the whole node now averages under a milliamp and the panel makes
eighty times what it uses.

**Keep the gate anyway, for a different reason.** There are **four** unshielded
5 m I2C branches running through a misted mushroom house, and the characteristic
I2C failure is a glitch that latches SDA low until something clocks the bus free.
Power-cycling the rail is the one recovery that always works. The gate is the
node's **I2C and 1-Wire reset button**, and firmware should use it that way: on a
failed transaction, gate off, wait, gate on, retry.

> **Everything the front-end reads is switched, including CO2.** That is worth
> stating plainly because it is what makes the gate a complete recovery mechanism
> rather than a partial one: there is no sensor on this board that `SENS_GATE`
> cannot power-cycle, and no always-on rail to reason about. It was not true
> before revision 2.0 — see *The CO2 sensor*.

Upstream of the six probes, all four I2C branches and the bus switch that fans
them out — which is now everything:

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

**The 1000 ms window was never real.** The firmware carried a 1000 ms SCD41
power-up constant, and this document repeated it as fact — it dominated every wake
and the sequencing was built around overlapping it with the 750 ms DS18B20
conversion. **SCD4x v1.7 Table 7 says the power-up time is 30 ms max**, and nothing
in that revision supports 1000. The constant was corrected in 2026-09; the
overlapping remains, because it is good practice and now costs nothing at all.
The SHT45 needs about a millisecond, so `I2C_POWER_SETTLE_MS` stays at **10 ms**
and rail-on time is set by the **750 ms conversion**.

**What the SCD41 does add is measurement time, not settle time:** two 5 s single
shots, taking the gated window from ~1 s to **~11 s**. Averaged over the wake that
is 0.0136 Wh/day, the smallest line in §5's budget.

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

**What the gate carries, and the one number that sizes Q1.** `VSENS` feeds the six
probes (~9 mA during conversion), three SHT45s (µA), the mux (µA) — and the
**SCD41's 175 mA typ / 205 mA max bursts**, which came back onto this rail in
revision 2.0 along with the sensor. That 205 mA is now the figure Q1 must pass,
and the AO3401A does it with two orders of magnitude to spare. Its real job
remains **fault recovery** (§ *The rail gate*), not current handling — but it is
no longer true that nothing on this rail draws current.

Route **U7's 3.3 V** → Q1 → `VSENS` as a wide trace anyway. On a large board it is
tempting to let `VSENS` wander to reach six probe connectors and four branch
connectors — don't; run it as a spine with short stubs. **The stub to J12 matters
most**, because it is the only one that carries 205 mA peaks; give the CO2 branch
the shortest, widest path from Q1 that the layout allows.

---

## 4. Bill of materials

Split in two, because the board is no longer the whole design: parts on the
**front-end PCB** in the dry box, and parts that live **out at the sensors**.

> **Where to buy it:** [`sourcing-th.md`](sourcing-th.md) has manufacturer part
> numbers for everything non-generic, what to check when substituting, and which
> items are better bought from local Thai suppliers than from Digi-Key TH.

> **Drawing it:** [`pcb-altium.md`](pcb-altium.md) takes this table into Altium
> Designer 17 — project layout, the footprint each reference needs, placement
> against §5's rules, and the fabrication outputs. It also audits the schematic
> sheets that already exist against this section.

### 4a. Front-end PCB

| Ref | Part | Value / spec | Notes |
|---|---|---|---|
| J7 | Box header 2×19, shrouded | 2.54 mm, keyed, latching | Signal cable to Nucleo CN10. **15 signals** in revision 2.0 |
| J8 | Power connector 2-pin | keyed, latching (JST-XH, Micro-Fit 3.0) | **3.3 V buck output** + GND to Nucleo CN6-4/6 |
| J14 | Solar input 2-pin | keyed, latching, ≥5 A | **24 V bank in** from the charge controller |
| J1–J6 | Pluggable screw terminal, 3-pin | Phoenix MC 1,5/3-ST-3,5 or clone | **One per probe** — six of them |
| J9–J12 | Pluggable screw terminal, 4-pin | same family | **One per I2C branch — four identical connectors, one part number.** `V / SDA / SCL / G`. Silkscreen the destination: `CH0 หัว`, `CH1 ท้าย`, `CH2 นอก`, **`CH3 CO2 กลาง`** |
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
| U3 | **TCA9548A** | I2C `0x70`, 8-ch bus switch | A2/A1/A0 to GND. **Channels 0–3 used** — three SHT45s and the SCD41. Nothing sits upstream of it |
| R23 | Resistor 0805 | 10 kΩ | U3 `RESET` pull-up to `VSENS`. **Not optional** — active-LOW, no internal pull-up |
| R15, R16 | Resistor 0805 | 4.7 kΩ | **Upstream** SDA/SCL pull-ups. Board-local, 30 mm — 4.7 kΩ is right here |
| R17–R22 | Resistor 0805 | **2.2 kΩ** | **Downstream**, one pair per channel. **2.2 kΩ, not 4.7 kΩ** — each pair drives 5 m of cable |
| C8 | Ceramic | 100 nF | U3 decoupling |
| R39, R40 | Resistor 0805 | **2.2 kΩ** | Channel 3's downstream SDA/SCL pull-up pair — the SCD41 branch. Same value and same reason as R17–R22 |
| **Power — input protection (§5)** ||||
| Q2 | P-MOSFET, ≥60 V | **DMP6023LE-13**, SOT-223 | Reverse polarity on the 24 V input. **Drain (and tab) to J14, source to the load** — see *Q2 — the reverse-polarity FET is wired backwards on purpose*. **Not** a 30 V part |
| R38 | Resistor 0805 | 470 kΩ | Q2 gate → GND. Turns the FET on **and** limits D10's current — the two are a pair |
| D10 | Zener, 250 mW | **12 V** (BZX84C12 SOT-23 / MMSZ5242B SOD-123) | Q2 gate → source, cathode at source. **Not optional** — without it V_GS reaches −32 V against a ±20 V limit |
| D9 | TVS unidirectional | **SMBJ33A** (33 V standoff, 600 W) | Across the 24 V input, **after Q2** — that is what lets it be unidirectional |
| F1 | Fuse, **time-lag (T)**, 5×20 mm cartridge preferred | 2 A, **I²t ≥ 0.5 A²s** | 24 V input. Sized by I²t, not amps: hot-plug inrush through Q2's body diode is ~0.1 A²s and will nuisance-blow a low-I²t 1206 |
| C11 | Electrolytic / polymer | 100 µF, **≥63 V** | 24 V bulk, before the modules. **63 V, not 50 V** — D9 clamps as high as 53.3 V |
| **Power — the buck module (§5)** ||||
| U7 | Buck module, **fixed 3.3 V** | Traco **TSR 1-2433**, SIP-3 through-hole | 4.75–36 V in, 1 A, ±2 %. **The only regulator on the board:** MCU (J8) + `VSENS` + everything on it. Pins **1 = +V_in, 2 = GND, 3 = +V_out** (datasheet rev. 2026-07-02); **no traces under the module**. **Fixed output only** — never an adjustable module with a trim pot; CN6-4 and the STM32WL die at 3.6 V |
| C19 | Ceramic X7R 1210, or small electrolytic | **22 µF, 50 V** | `C_IN`, **directly across pins 1 and 2**, ≤5 mm. **22 µF is Traco's requirement, not a choice** — the datasheet demands an external 22 µF/50 V input capacitor for V_in > 32 V |
| C21 | Ceramic X7R | 22 µF, 16 V | `C_OUT`. **Optional** — the module needs none; fitted for the LoRa TX and SCD41 current steps. DNP without consequence |
| — | LED + series resistor | ×1 | **DNP.** Rail indicator for bring-up only — ~0.5 Wh/day if left fitted, which is more than half this node's entire budget |
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
| TP1–14 | Test pads | — | `VSENS`, all six DQ, `SENS_GATE`, **`24V_PROT`**, `VBAT_SENSE`, `GND`, upstream SDA/SCL |
| TP15–22 | Test pads | — | The **four** downstream SDA/SCL pairs. Without these, a dead sensor and a dead mux channel look identical |
| TP23, TP24 | Test pads | — | **V_in and V_out of U7.** With `24V_PROT` at TP1 they tell a dead module from a dead input in one measurement, and TP23 is where the no-load current gets measured |
| TP25, TP26 | Test pads | — | **`24V_RAW`** (J14 side of Q2) and **Q2's gate**. With `24V_PROT` at TP1 these three tell a blown F1, a dead Q2 and a missing gate clamp apart in one measurement |

**Deleted from the previous revision:** U2 (SCD41), C3 (its 10 µF local bulk), C7
(its 100 nF), and U1a/U1b/U1c with C4–C6 — the SHT45s are no longer on this board.

**Deleted in revision 1.0 (2026-09), with the LM5164 bucks:** L1, L2, C13, C16–C18,
C20, C22–C25, R26–R37 — the inductors, bootstrap caps, `R_RON`/feedback/ripple-
injection networks, the shared EN/UVLO divider and the `PGOOD` pull-ups. About
thirty parts. All of it is still documented in
[`hardware-interface-back.md`](hardware-interface-back.md).

### 4b. Out at the sensors — not on the PCB

| Where | Part | Notes |
|---|---|---|
| Each probe ×6 | **100 nF** | Across that probe's VDD/GND at the **far** end |
| Each SHT45 ×3 | **SHT45-AD1B** + **100 nF** | Cap across VDD/VSS at the sensor. Small vented housing |
| SHT45 #2 only | **Radiation shield** | Louvered/Stevenson type, sensor below, north-facing. Without it the ambient reference reads 10–15 °C high in sun |
| CO2 head | **Sensirion `SEK-SCD41-SENSOR`** breakout (recommended), or a bare **`SCD41-D-R2`** on a small head PCB | Vented, splash-protected housing, **diffusion opening facing down**, PTFE/Gore membrane, **never sealed**, and **not in a misting nozzle's path**. **No radiation shield** — a mushroom house is dark |
| CO2 head | **100 µF, low-ESR** + **100 nF** | Across VDD/GND at the sensor. Not optional, and **low-ESR is part of the spec**: this capacitor is both the 205 mA reservoir and the shunt leg of the ripple filter that lets a switching regulator feed a part that asked for an LDO (§3, *The 30 mV question*) |
| CO2 head | **2.2 kΩ ×2** | Channel 3's pull-up pair, if not already on the breakout |
| CO2 head | **Heater resistor pad — DNP** | ~220 Ω across the branch 3.3 V (~50 mW). **Lay out the pad, fit nothing.** The SCD41 self-heats only ~1.5 mW, so it cannot hold itself above the dew point; whether it needs help is a question for the SHT45 logs. See *Condensation* in §3 |
| CO2 head | **VDD and VDDH bridged** | Mandatory, both from the same rail, **linked close to the sensor**. Already done on the breakout; easy to miss on a bare LGA |
| CO2 head, bare LGA only | **21 floating pads** | Every DNC pad must be soldered to a pad that goes nowhere — see *Building a bare LGA head* in §3 |

---

## 5. Power — 24 V solar

**This replaced the battery entirely in 2026-08.** The node is fed from a **24 V
battery bank behind a charge controller**, not from cells, and that change
propagates further than anything else in that revision: it is what broke
`battery_read_mv()`, and it is why the parts after J14 are rated for 60 V rather
than for the 5 mA the node actually draws.

### What the node must survive

| Parameter | Value | Why |
|---|---|---|
| Nominal | 24 V | 2× 12 V lead-acid, or 8S LiFePO4 |
| Normal range | **21–29 V** | discharged → absorb/equalise |
| **Design range** | **18–32 V continuous** | margin both ways |
| **Part rating** | Input chain (Q2, D9, C11) **≥60 V** as before; U7 **36 V max** | the module is the ceiling now. A controller fault, a disconnected battery, or a cold-morning panel V_oc can put far more than 29 V on the wire, and **nothing on this board stops that** — see *What the input chain no longer covers* in *The buck modules* |

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
The node draws well under **1 mA average and ~30 mA peak** at 24 V (the LoRa TX's
150 mA and the SCD41's 205 mA are reflected through the module, so they arrive
here divided by roughly 24/3.3). Every current axis is over-specified by more than
two orders of magnitude.

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
      ├──► D9, C11, U7         └──[ R38  470 k ]─── GND
      │    (all in PARALLEL — see the next section)
      │
   J14 (−) ─────────────────────────────────────── GND
```

Reading it as a netlist, because the drawing above is the part people get wrong:

| Terminal | Net | Note |
|---|---|---|
| **Drain** (and tab) | `24V_RAW` — the J14 side, after F1 | Yes, the drain faces the supply |
| **Source** | `24V_PROT` — the load side, feeding D9/C11/U7 | |
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
blow F1 with luck and destroy the modules without it. **This failure is invisible
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
snaps. That is a small bonus for EMI and for the modules' input step, but read the
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
  and the module's ceramic input capacitor.

> **Q2 is not an ideal diode and does not block reverse *current*.** Once enhanced,
> the channel conducts both ways. If the bank voltage ever falls below `24V_PROT`,
> C11 discharges back into it. That is harmless at these energies and is worth
> stating only so nobody designs on the assumption that it does not happen.

#### Two traps

**1. Hot-plug inrush goes through the body diode, not through the channel — so the
1.2 ms gate ramp is not a soft-start.** Connecting a charged bank to a discharged
board dumps ~2.6 mC into `C11` plus the module's 22 µF of local input capacitance,
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

### The `24V_PROT` node — D9, C11 and the module are all in parallel

**"F1 → Q2 → D9 → C11" describes physical order along the trace, not an electrical
chain**, and the shorthand has caused enough confusion to be worth replacing with a
drawing. Only **F1 and Q2 are series elements**. Everything after Q2 hangs off a
single node:

```
   Q2 source
       │
       ●━━━━━━┳━━━━━━━━┳━━━━━━━━━━━━┳━━━━━━━━━━┳━━━  24V_PROT
       │      │        │            │          │
      TP1   ══╪══     ═╪═         ══╪══     [R24 300k]
              │        │            │          │
            [D9]     [C11]       [C19]         ●──── VBAT_SENSE
           SMBJ33A   100 µF        22 µF       │      → PB3
          cathode ↑   + ↑         50 V     [R25 30k]
              │        │            │          │
              │        │        U7 V_in        │
              │        │        (TSR 1)        │
       ───────┴────────┴────────────┴──────────┴──── GND
              ↑        ↑            ↑
          at J14   next to it   ≤5 mm from
                                 U7's pins
```

Read as a netlist, since polarity is where this node gets destroyed:

| Part | Terminal | Goes to |
|---|---|---|
| **D9** SMBJ33A | **cathode — the banded end** | `24V_PROT` |
| | anode | `GND`, straight back to J14's ground pin |
| **C11** 100 µF ≥63 V | **`+`** | `24V_PROT` |
| | `−` | `GND` |
| **C19** 22 µF/50 V | — | across **U7's** pins 1 (+V_in) and 2 (GND) |
| **U7** V_in (pin 1) | — | `24V_PROT` |
| **R24** 300 kΩ | — | `24V_PROT` (top of the `VBAT_SENSE` divider) |

**One module taps this node, and the layout rules below still matter.** With a
single converter there is no question of rails sharing an input — but D9's surge
current and R24's high-impedance analog tap share this copper, and that is what
the ordering rules are actually about. (An older revision had a second module
hanging here beside U7; a board with two is not this design.)

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
3. **The module's `C_IN` within 5 mm of its V_in/GND pins.** C11 cannot do
   this job — see *The buck modules*, layout rule 1. The converse also applies: a
   module fed through more than a few centimetres of trace, or any length of cable,
   wants bulk capacitance upstream — which is what C11 is for.
4. **R24/R25 last**, farthest from the module. It is a 27 kΩ analog source; it
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
module's 22 µF of local input capacitance at roughly **15 kHz**, and connecting a
charged bank rings that circuit. Cable resistance alone (~0.2 Ω for 5 m of 18 AWG,
round trip) already supplies most of the damping, and D9 backstops whatever is
left — so this is margin, not a crisis. But **C11's job here is damping, not ripple
current** (the local ceramics carry the 500 kHz ripple), so its ESR is a feature.
Prefer the **aluminium electrolytic** over the polymer, and read the "≥63 V"
requirement as covering both D9's 53.3 V clamp *and* this ring.

### One rail, and why the second one went away

```
  24 V ──[F1]──[Q2 rev]──[D9 TVS]──┬──[U7 TSR1-2433]── 3.3 V ──┬── J8 ─> Nucleo CN6-4
                                   │                           │
                                   │                           └──[Q1 gate]── VSENS
                                   │                               (probes, SHT45s,
                                   │                                mux, SCD41)
                                   └──[R24/R25]── VBAT_SENSE ─> PB3 (ADC1_IN2)
```

**Every load on this board runs from 3.3 V, so the board makes 3.3 V and nothing
else.** That is the whole of the power architecture. A second rail existed until
revision 2.0 only because the CO2 sensor of the day needed 4.5–5.25 V; nothing on
the current BOM does.

An earlier revision also argued that a separate rail protected the radio from a
faulty CO2 branch. That argument does not survive the change, and it is worth
saying why rather than letting it lapse quietly:

- **The fault current is ten times smaller.** A shorted CO2 branch is a 205 mA
  load, and the TSR 1-2433 current-limits at 250 % of 1 A and recovers by itself.
- **The exposure was never unique to CO2.** Three SHT45 branches, each 5 m of
  unshielded cable in the same building, have always been on the 3.3 V rail. A
  fourth branch of the same kind is not a new class of risk.
- **It is gate-recoverable.** Everything on `VSENS` can be power-cycled by
  firmware. A rail the gate cannot reach is the case that needs a site visit, and
  this design no longer has one.

> **U7's output must never exceed 3.6 V.** CN6-4 is rated **3.0–3.6 V** (UM2592
> Table 9) and the STM32WL's absolute maximum VDD is 3.6 V. Use a fixed 3.3 V part,
> not an adjustable one with a trim pot somebody can turn.

### The buck module — U7

One off-the-shelf **Traco TSR 1** module supplies the whole board. (Two earlier
revisions got here: rev 1.0 replaced a pair of discrete LM5164 converters with a
pair of modules, and rev 2.0 dropped one of the pair.)

| | **U7** |
|---|---|
| Part | Traco **TSR 1-2433** |
| Output | **3.3 V ±2 %** (3.23–3.37 V) |
| Input range | 4.75–36 V |
| Output current | 1 A |
| Load here | MCU + radio (~150 mA peak) and all of `VSENS` (SCD41 205 mA peak, probes ~9 mA) — **never simultaneously**, see below |
| Short-circuit | continuous, auto-recovery; current limit **250 %** of I_out typ |
| Over-temperature | 150 °C internal, auto-recovery |
| No-load input current | **1 mA typ** |
| Output ripple | 50 mV_pp typ (20 MHz BW) |
| Switching frequency | 500 kHz typ (400–600) |
| Max capacitive load | 470 µF |
| Package | SIP-3 through-hole, 78xx pin layout, 11.7 × 7.5 × 10.1 mm |
| Operating temperature | −40…+85 °C |
| Price / source (2026-09-02) | ฿206.49, Digi-Key TH |

**The two big loads never coincide, and that is by construction.** `main.cpp`
closes the gate — which removes the SCD41 and the probes from the rail entirely —
*before* the radio is powered for the uplink. So the module sees ~205 mA while
sensing and ~150 mA while transmitting, not 355 mA. A 1 A part has room either
way, but the sequencing is load-bearing and §7 tests it.

**U7's worst case is 3.37 V, under the 3.6 V ceiling of CN6-4 and the STM32WL.**
That is why it must be the fixed-output part and never an adjustable module: the
number that protects the MCU is a factory trim, not something a pot can be turned
to.

#### Pins

Three pins in 78xx order, confirmed from the TSR 1 datasheet (rev. 2026-07-02,
*Pinout* table). Traco adds one layout instruction of its own: *"avoid routing PCB
traces under the converter."*

| Pin | Name | Connect to |
|---|---|---|
| 1 | `+V_in` | `24V_PROT`. **`C_IN` (22 µF / 50 V) directly across pins 1 and 2** |
| 2 | `GND` | Ground plane, with its own wide copper back toward J14's ground pin |
| 3 | `+V_out` | The 3.3 V rail: C21, J8, and Q1's source |

There is no enable, no power-good and no feedback pin — nothing to tune, nothing
to scope, and no COT ripple network to get wrong. That was the point of revision
1.0 and it still holds.

```
                        ┌───────────────┐
   24V_PROT ────┬───────┤1 +Vin  +Vout 3├───────┬─────────── 3.3 V
                │       │               │       │
            [ C_IN ]    │  TSR 1-2433   │   [ C_OUT ]
           22 µF/50 V   │      U7       │    22 µF
                │       │    2 GND      │       │
               GND      └───────┬───────┘      GND
                               GND
```

#### Capacitors

- **`C19` — 22 µF / 50 V, across pins 1–2 within 5 mm.** The value is Traco's, not
  ours: the datasheet requires *"an external input capacitor 22 µF / 50 V for input
  voltage higher than 32 VDC"*, and 32 V is the top of the design range. Either a
  1210 X7R (accept ~40 % DC-bias loss at 32 V) or a small aluminium electrolytic.
- **`C21` — 22 µF / 16 V, optional.** The module is internally compensated and
  needs no output capacitor. Fitted because it is cheap and takes the edge off the
  LoRa TX and SCD41 current steps; DNP it without consequence.
- **`C11` — 100 µF / ≥63 V electrolytic, keep, and keep the voltage rating.** It
  sits on the clamped side of D9, which reaches 53.3 V during a surge. That the
  module is a 36 V part does not lower what C11 sees.
- **There is no bulk capacitor on the board for the CO2 sensor, and that is
  deliberate.** Its reservoir is the **100 µF at the head** — on the far side of
  the cable, which is the only side that can actually keep a 205 mA burst off it
  (§3, *The 205 mA problem*). An earlier revision carried a `C15` here for the
  same job and it was on the wrong end of 5 m of wire.

#### Layout — four rules

1. **`C_IN` across pins 1 and 2, within 5 mm.** The switching loop is inside the
   module, but its on-board input decoupling is small and the 500 kHz input ripple
   current still has to come from somewhere close.
2. **The module in one corner, with the 24 V input, far from J1–J12.** Six
   bit-banged 1-Wire lines, four unshielded 5 m I2C branches, a 27 kΩ ADC divider
   and a 923 MHz radio all live on this board.
3. **Give pin 2 its own wide GND return to J14.** It is the only ground pin the
   module has; do not let it share a thin trace with the analog ground or R25.
4. **Nothing routed under the module** — Traco's own instruction. The body sits
   0.5 mm off the board; keep that footprint area copper-free on the top layer.

#### What the input chain no longer covers

The LM5164 revision demanded **≥60 V** on every part after F1 because *"a
controller fault, a disconnected battery, or a cold-morning panel V_oc can put far
more than 29 V on the wire"*. That is still true, and the module is a **36 V
part**. Be clear about what F1/Q2/D9/C11 do and do not do for it:

- **D9 (SMBJ33A) handles surge *energy*, not overvoltage.** It stands off 33 V and
  clamps at up to **53.3 V** while doing so — above the module's rating. No TVS
  that stays off at 32 V clamps below ~45 V; that is TVS physics, not a wrong part
  number. D9 keeps a lightning-induced or switching transient from destroying the
  board; it does not keep the module inside its rating during one. The datasheet
  publishes no absolute maximum beyond the 36 V operating limit, so treat 36 V as
  the ceiling.
- **A charge controller that fails and passes panel V_oc (~44 V for a "24 V"
  panel) through kills the module.** Nothing passive on this board prevents that,
  and adding an active OVP disconnect would put back the kind of circuit
  revision 1.0 exists to remove. **This is a known limitation.** The mitigation is
  procedural and free: before the board is ever connected to the real system,
  measure the panel's V_oc and confirm the controller does not pass it through
  with the battery disconnected (§7, *before step 1*).
- **Everything else the chain does is unchanged:** F1 sized by I²t for hot-plug
  inrush, Q2 for reverse polarity, C11 for damping the cable resonance.

#### Two traps specific to this module

**1. Do not substitute a 28 V module.** DFRobot DFR0570/0571, MP1584 and LM2596
boards are all rated 28 V maximum input. A 24 V lead-acid bank sits at **28.8 V on
absorb**; an 8S LiFePO4 bank reaches **29.2 V** full. That is not a fault case, it
is every sunny afternoon. The failure is slow: the module survives 24.0 V on the
bench and dies the first time the bank hits absorb.

**2. The pin order is 78xx — 1 = +V_in, 2 = GND, 3 = +V_out — confirmed, but buzz
the footprint out before the module is soldered anyway.** A reversed footprint
puts 24 V on the output pin, and the Nucleo is downstream of it.

### 3.3 V goes down the CO2 cable, and the wire gauge is part of the spec

The CO2 branch carries the same 3.3 V as the three SHT45 branches and is
electrically identical to them — with one exception worth a paragraph, because it
is the only place on this board where wire gauge is a specification rather than a
preference.

**The SCD41 draws 175 mA typ / 205 mA max in bursts.** Over 5 m:

| Gauge | Ω/km | Round-trip R over 5 m | Drop at 205 mA | VDD at the head |
|---|---|---|---|---|
| 24 AWG single conductor (as used for the SHT45s) | 84.22 | 0.84 Ω | 173 mV | 3.06 V |
| 22 AWG | 52.96 | 0.53 Ω | 109 mV | 3.12 V |
| **2× 24 AWG in parallel — specified (§6)** | **42.11** | **0.42 Ω** | **86 mV** | **3.14 V** |
| 20 AWG | 33.31 | 0.33 Ω | 68 mV | 3.16 V |

**The specified branch is Cat5 with `V` and `G` doubled up**, not a separate 22 AWG
cable. Cat5 has eight conductors and this branch needs six; spending two of the
spares on copper beats buying a second cable type, and it lands *below* 22 AWG.

(VDD at the head is U7's ±2 % worst case, 3.23 V, minus the drop.)

All three clear the SCD41's **2.4 V** minimum with enormous margin, so this is not
about the sensor browning out. It is about the datasheet's request that the supply
*"not vary by more than 30 mV (e.g. ripples or drops caused by other loads)"* and
its recommendation of a dedicated LDO.

**There is no LDO, and the 30 mV is nonetheless met — at the sensor, which is
where the datasheet specifies it.** U7 puts out 75 mV p-p, and the cable and the
head capacitor between it and the SCD41 attenuate that to roughly 2 mV. The
arithmetic, the assumption it rests on, and the bring-up step that checks it are
in §3, *The 30 mV question*; do not re-derive it here. What §5 owes that argument
is the wire: **22 AWG is load-bearing twice over** — low resistance for the IR
drop above, and enough resistance to damp the filter's LC.

**Why not send a higher voltage and regulate at the head?** Because it would put a
second regulator on the board — its capacitors, and half of this node's idle
current — to solve a 109 mV problem that a thicker wire already solves. The
textbook answer is right when the sensor needs a voltage the board does not
generate. This one does not.

### The energy budget, and why it is now genuinely uninteresting

| Load | Draw | Per day |
|---|---|---|
| **U7 no-load input current** | 1 mA typ × 24 V = 24 mW | **0.58 Wh** |
| MCU + LoRa + probes + SHT45s | ~1 mA avg @ 3.3 V | <0.1 Wh |
| Module conversion loss | | ~0.1 Wh |
| **SCD41** — 96 power-cycled single shots | 154 mC per useful shot @ 3.3 V | **0.0136 Wh** |
| TCA9548A standby | 0.1 µA typ | ~0 |
| ST-LINK, **if** you power it | ~5 mA @ 3.3 V | ~0.4 Wh |
| **Total** | | **≈0.8 Wh/day** |

Against ~80 Wh/day from a 20 W panel at four peak-sun hours, the whole node is
**~1 %** of the yield. Overnight carry is 12 h × ~33 mW ≈ **0.4 Wh**; three cloudy
days is ~2.4 Wh, against a 20 Ah 24 V bank's 480 Wh.

**Read the shape of that table, not just the total.** **72 % of the budget is a
regulator doing nothing**, and the CO2 sensor — which dominated every earlier
version of this design, at 2.16 Wh/day — is **under 2 %**. There is no load left
here worth optimising: halving the sensor would buy 0.007 Wh/day against a panel
that makes eighty. The next watt-hour, if one is ever spent, should go on the head
heater (§3, *Condensation*), which takes the total to ~2.0 Wh/day and is still
less than this board drew before the 5 V rail was deleted.

### Three traps

**1. `battery_read_mv()` no longer works, and the fix is a divider.** VREFINT
measures VDDA — which is now U7's regulated 3.3 V output, a number that is the same
whether the bank is full or nearly flat. §2 predicted this: *"add a regulator
between battery and MCU and it starts reporting the regulator output instead."*
The replacement is **R24/R25 (300 kΩ / 30 kΩ, 11:1) → PB3 (ADC1_IN2)**, giving
2.9 V at a 32 V bank. C10 across R25 makes the source low-impedance at the sampling
instant; 300 k‖30 k is 27 kΩ, which needs a long ADC sampling time.

> **VREFINT is still needed — for a different job.** Read it to recover the *actual*
> VDDA, then scale the divider reading against that. Otherwise the module's 2 % tolerance
> becomes 2 % of error on every bank-voltage report. The firmware keeps VREFINT and
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
the bank, which at ~0.8 Wh/day it emphatically will not: a 20 Ah bank outlasts a year of sunless
days. The LM5164 revision kept a 15 V UVLO under this as a last backstop; the TSR 1
has none and runs down to 6.5 V, so the node now reports right down to the bank's
death — which, for a node whose job includes measuring the bank, is the behaviour
you want.

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
| 10 | **SCD41 — กลางโรงเรือน, at crop level** | 5 m, **22 AWG** | `V / SDA / SCL / G` |
| 11 | Solar input | — | 24 V + GND from the charge controller |

**Eleven cable entries**, and **four of them are now the same cable with the same
connector**: `V / SDA / SCL / G` on a 4-pin pluggable terminal, J9 through J12.
Revision 2.0's CO2 branch is not a special case any more — it is the fourth I2C
branch, differing only in wire gauge. That, not the PCBs, sets the enclosure size,
the same lesson this document already learned once when the probe count went to six.

**The building is a mushroom house (โรงเรือนเพาะเห็ด), not a plant greenhouse**,
and three things follow that are easy to get wrong if you inherit text from the
previous revision: it is **dark** (no radiation shields inside, no solar load),
it is **misted** (liquid water lands on surfaces as a matter of routine, not as an
accident), and it is **CO2-rich by design** (nothing in it ever approaches outdoor
air, which is what §3's ASC section is about).

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
- **I2C branches: ≤5 m, all four, and this is a hard number.** At 5 m with 2.2 kΩ
  the rise time is 0.60 µs against a 1000 ns budget. At 10 m it fails. If a run
  must be longer, fit a P82B715 on that branch (§3) — do not simply lengthen the
  cable. Twist **SDA with GND**; run SCL in the second pair. One Cat5 per branch.
- **The CO2 branch needs more copper on `V`/`G` than a single Cat5 conductor**, and
  that is a specification, not a preference — it is the only branch carrying a
  205 mA burst, and a lone 24 AWG conductor drops 173 mV against 22 AWG's 109 mV.
  **Preferred: keep Cat5 on all four branches and double up conductors** — Cat5
  has eight, so `V` and `G` get two each, `SDA` and `SCL` one each, and two spare.
  Two 24 AWG conductors in parallel are **42.11 Ω/km**, which beats 22 AWG's 52.96
  outright: 0.42 Ω round trip and an **86 mV** drop. One cable part number covers
  the whole build, and the CO2 branch ends up with *better* margin than the
  original specification. A dedicated 4-core 22 AWG cable is the alternative if
  doubling up at the terminals is awkward. See §5, *3.3 V goes down the CO2 cable*.
- **Label both ends of every branch.** Four identical connectors and four
  near-identical cables. A swapped J10/J11 silently exchanges "inside the house"
  for "ambient reference" and the data still looks plausible; a swapped J12 puts
  the CO2 sensor's 205 mA burst down a 24 AWG cable and nothing complains at all.
- **Sensor housings — each location needs its own.** Small, vented, sensor facing
  down. The SHT45s tolerate 100 %RH and recover from condensation, so they need
  protection from *liquid* water and sun, not hermetic sealing.
- **SHT45 #2 needs a radiation shield, not an enclosure** — it is the only sensor
  that sees sun. Louvered or Stevenson-type, sensor below the shield, north-facing.
  An unshielded sensor in Thai sun reads **10–15 °C above true air temperature** and
  the ambient reference becomes worse than useless — it becomes misleading. **The
  three sensors inside the house need no shield**; it is dark in there.
- **The CO2 head is the demanding one.** Vented, splash-protected housing with the
  **diffusion opening facing downward** so condensate drips away rather than
  pooling on the optical path, a **PTFE/Gore membrane**, **never sealed**, and
  **not in the direct path of a misting nozzle**. No radiation shield. The SCD41 is
  rated 0–95 %RH non-condensing and this building will sit against that limit —
  see *Condensation* in §3 for the heater pad that may have to be fitted.
- **The front-end enclosure does not need venting.** Nothing in it measures air.
  It should be **IP65 and as sealed as the glands allow**. Keep the desiccant habit
  anyway if the box is opened often in humid weather.
- **RF:** keep the antenna clear of metal, and keep the ribbon away from the RF
  section and the CN12 SMA. A 30 cm ribbon draped over the antenna is a real
  antenna-detuning problem — and there are now eleven cables competing for the same
  space, so plan the routing rather than discovering it.
- **Test points:** the ones in the BOM — and note that TP15–22 are now **four**
  downstream SDA/SCL pairs, one per branch. With six probes and four I2C branches,
  "which one?" is the first question of every field failure.

---

## 7. Bring-up order

Do this before the boards go in a sealed box on a pole. The
[`bluepill_f103c8_dump`](../README.md) diagnostic is the right first power-on test
and ports to the WL55 in a few lines.

**Power first, and on its own** — this is new, and it is the step that protects
everything else:

> **Before step 1 — once, at the real installation, with a meter.** Measure the
> panel's open-circuit voltage at the controller's PV terminals, then disconnect
> the battery and check that the controller's battery/load terminals do **not**
> follow it. A "24 V" panel's V_oc is ~44 V; U7 is a 36 V part and nothing on
> this board stops a controller that passes V_oc through. Five minutes, and it is
> the only defence there is — *What the input chain no longer covers*.

1. **Bring up the 24 V front end with no PCB loads.** F1 → Q2 → D9 → C11, then
   confirm **U7 = 3.3 V ±2 %** on a bench supply swept **18 → 32 V**. It must never
   read above **3.6 V** at any input voltage; if it does, stop — CN6-4 and the
   STM32WL are both absolute-max 3.6 V. A DMM is enough for the rail; a scope on
   the output should show **≈50 mV_pp** of ripple (datasheet typical) at ~500 kHz
   and nothing slower. There is no `SW` node to inspect and no COT bursting to
   catch — the converter is inside the module.
   **Measure and record the no-load input current** here, from the bench supply's
   readout with the output unloaded: §5's energy table uses the datasheet typical
   of 1 mA and no maximum is published, and that single line is **72 % of this
   node's entire budget**. Write the measured number into that table.
2. **Deliberately short `VSENS` at J12.** U7 should current-limit and recover when
   the short is removed. What this proves is narrow but worth having: **a shorted
   sensor branch does not destroy the module**, and `SENS_GATE` clears it. (An
   earlier revision ran this test at a 5 V rail, to show that a CO2 fault could not
   brown out the radio. With one rail that framing is gone; the recovery path is
   what matters.) Drop the gate
   with the short still applied and confirm the rail recovers — that is the
   recovery path firmware depends on, and it is the reason the CO2 sensor was
   brought back behind the gate.
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
   the **fifteen** contract positions and, critically, that **nothing** rings out
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
   pull-ups. **Everything must go to 0 V** — there is no ungated sensor rail in
   this revision, and anything still alive with the gate high is a pull-up that
   landed on permanent 3V3.
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
    else**. No `0x44` and no `0x62`: either one answering here is a sensor wired
    upstream of the switch, which will collide (0x44) or permanently load the bus
    with 5 m of cable (0x62) the moment a channel opens.
15. **Scan again with each channel open in turn** → exactly one `0x44` on channels
    0, 1 and 2, and exactly one **`0x62` on channel 3**.
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
17. **Read the SCD41's serial number on channel 3, with the real 22 AWG cable
    fitted.** `get_serial_number` (0x3682) is the proof-of-life check and
    `scd41_init()` already does it. This separates "the branch works" from "the
    sensor measures", which are different faults with different fixes — and it is
    the step that catches a CO2 branch accidentally built with 24 AWG Cat5 like the
    other three.
18. **Read CO2, and check the three settings the firmware is responsible for.**
    The debug log prints `co2=… asc=0 t=… rh=…`:
    - **`asc=0`.** If it reads 1, `scd41_ensure_asc()` did not take, and the sensor
      will slowly calibrate itself into uselessness in a house that never sees
      400 ppm. This is the single most important number in the whole bring-up.
    - **Altitude** — read it back with `get_sensor_altitude` (0x2322) and expect
      `SCD41_SITE_ALTITUDE_M` (330). Set before any FRC, never after.
    - **The value itself.** Outdoor air is **~420 ppm**. A mushroom house runs
      *higher* than outdoors at all times and highest when closed up — the opposite
      of a plant greenhouse, so do not use the old intuition. During spawn run it
      will read past 5000 ppm, which is outside the specified accuracy but on
      scale; see §3.
    - **The SCD41's own temperature** against the nearest SHT45. It reads high by
      design (it self-heats), but *far* high means the head is not ventilating.
    A failed CRC is a wiring problem; a valid reading that is implausible is a
    calibration problem, and the answer to that is the FRC procedure in §3 — not
    turning ASC back on.
19. **Wiggle test.** With everything reading, flex the ribbon and tug each of the
    eleven cable entries. Nothing should glitch. This is the test that finds a
    marginal IDC crimp before the pole does.
20. Full cycle → wake, read, TX, sleep. Confirm the gateway logs `6/6 probes and
    3/3 air sensors reading` plus a CO2 value, and confirm `VSENS` actually goes
    off **during** the sleep rather than at the next wake. Time the gated window
    while you are there: it should be **~11 s**, dominated by the SCD41's two 5 s
    single shots. Much shorter means the warm-up shot is being skipped; much
    longer means something is retrying.
21. **Scope `VSENS` through one full wake, with the CO2 branch connected.** This is
    the one measurement that is new in revision 2.0 and has no equivalent in the
    previous revisions: the SCD41's **205 mA bursts** are now on the same rail as
    everything else. Confirm they land *after* the DS18B20 conversions and the
    SHT45 reads have finished — if they overlap, the ordering in `main.cpp` has
    been changed and the probes are being sampled through a disturbed supply.
22. **Scope VDD *at the CO2 head*, and check two numbers there.** §3's *30 mV
    question* argues that the 5 m cable filters U7's switching ripple down to
    roughly 2 mV at the sensor, but that argument rests on an *estimated* cable
    inductance. This is where the estimate gets tested. Probe across the head's
    100 nF, ground clip short, and read:

    | | Expect | If it fails |
    |---|---|---|
    | **Ripple, sensor idle** (AC-coupled, 20 MHz BW limit) | **< 30 mV p-p**, and the calculation says ~2 mV | Near or above 30 mV means the head capacitor is high-ESR, missing, or not actually at the sensor. This is the datasheet limit, not a guideline |
    | **Sag, during a shot** | Head VDD stays **well above 2.4 V** (predicted ~3.12 V) | A sag toward 2.4 V means 24 AWG got fitted instead of 22 AWG, or the 100 µF is absent |

    Both faults are silent: the sensor keeps answering and keeps returning numbers
    that pass CRC. Measure once, at build time, and write the two values in the
    build log — there is no runtime symptom that will bring you back here.

---

## References

- **UM2592** — STM32WL Nucleo-64 board (MB1389) user manual: the authority for
  this board. **Table 18** ("Pin assignment of the ST morpho connectors") is the
  source for §2's CN10 map and Appendix A's CN7 map; **Table 17** ("ARDUINO
  connectors pinout") for the CN6 power tap; **Table 9** ("External power sources:
  3V3") for the CN6-4 battery input; §6.6.5 and the solder-bridge tables cover the
  VCP/D0-D1 arrangement.
- Sensirion **SCD4x** datasheet, **v1.7 (April 2025)** — the CO2 sensor. §1.1
  accuracy bands, §2.1 the 175/205 mA peak and the single-shot average, §2.2 the
  −10…60 °C / 0–95 %RH envelope, §2.3 the pad table, the VDD/VDDH tie and the
  30 mV supply-quiet request, §2.4 the **30 ms** power-up, §3.8 **ASC and its
  weekly-400-ppm assumption** plus FRC, §3.11 single-shot mode and the note that
  **ASC is unavailable when power-cycled**, §3.12 the CRC-8 that
  `src/sensirion_i2c.cpp` implements, and §4.1–4.6 the package, land pattern,
  MSL 1 and the reflow profile.
  **Read §6, the revision history, as well** — two entries in it change how the
  rest should be read. v1.4 *"[corrected] power-up time and soft reset time"*
  (1000 ms → 30 ms; the old figure still circulates in Sensirion's own app note),
  and **v1.7 is the revision that removed the "discard the first single shot
  after a power cycle" recommendation that this build still follows** — see §3,
  *The cost is the throwaway shot*, for why it is still followed.
- Sensirion application note **"SCD4x Low Power Operation"**, v1.0 (July 2022) —
  the authority for the operating mode this node actually uses. §2.4
  power-cycled single shot, the discarded first reading, and the **380 s**
  threshold above which power-cycling beats idling; §3.3 **Equation 2** (154 mC
  per useful shot at 3.3 V), which is where §5's CO2 energy line comes from; §5.1
  the FRC run-up of **5 minutes at a 1-minute sampling period**. Treat its timing
  flowcharts with care — they still show the superseded 1000 ms power-up.
- Traco Power **TSR 1 series** datasheet, rev. 2026-07-02 — U7 (TSR 1-2433):
  input range, ±2 % set accuracy, 250 % current limit, 1 mA typ no-load input
  current, the SIP-3 pinout (1 = +V_in, 2 = GND, 3 = +V_out), the 470 µF
  capacitive-load limit, the **22 µF / 50 V input capacitor required above
  32 V_in**, and *"avoid routing PCB traces under the converter"*.
- TI **TCA9548A** datasheet (SCPS207H) — the bus switch. §5.5: standby I_CC of
  **0.1 µA typ / 2 µA max**, which is why putting four branches behind it costs
  nothing.
- Senseair **PSP14281 / TDE14367 / TDE15154 / PSP14808** — the S88 family, used by
  the previous revision and retained in
  [`hardware-interface-s88.md`](hardware-interface-s88.md). Not used by this build,
  but the **S88 GH** (0–20 000 ppm, 0–95 %RH) is the documented fallback if the CO2
  range requirement ever outgrows the SCD41.
- TI **LM5164** datasheet (SNVSAU4) and **AN-1481** — the discrete bucks of the
  previous revision, retained in
  [`hardware-interface-back.md`](hardware-interface-back.md). Not used by this build.
- Maxim/ADI **AN148** — guidelines for reliable long 1-Wire networks.
- DS18B20 datasheet — **VDD 3.0–5.5 V** is the constraint driving battery choice.
- Sensirion **SHT4x** datasheet — command set, timing, and the transfer functions
  ported in [`src/sht45.cpp`](../src/sht45.cpp).
- Sensirion **SHT4x** and **SCD4x** command sets are both ported by hand onto the
  shared word protocol in [`src/sensirion_i2c.cpp`](../src/sensirion_i2c.cpp) —
  16-bit big-endian words, each followed by CRC-8 (poly 0x31, init 0xFF). That is
  **not** the frame's `lora_crc8` (poly 0x07, init 0x00); the codebase carries two
  distinct CRCs and mixing them is a silent-corruption bug.

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
