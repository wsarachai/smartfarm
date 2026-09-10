# `STM32WL_PT` build sheet

**Print this at 1:1 and keep a copy in the enclosure lid.**

This board has no silkscreen, by decision (`pcb-home-etch.md`). That makes this
document the only thing on site that says which way round a part goes. Someone
opening the box in 2028 has this sheet and nothing else — so it travels with the
board, not just with the repository.

Referenced by `pcb-home-etch.md` Stages 11, 12 and 14, and by
`hardware-interface-proto.md` §7. Values come from
[`hardware-interface-proto.md`](hardware-interface-proto.md); if this sheet
disagrees with it, that one is right.

```
board ____ of 5      built by ______________      date ____________
etched ______  drilled ______  tinned ______  vias ______ / ____
§7 passed ______      coated ______
```

---

## 1. Before the modules are soldered — the pre-fit audit

Both modules plug into female header strip, so this is done with the module in
your hand and a multimeter, **before** anything is committed.

### U3 — TCA9548A breakout (SKU-0260-1)

The board is a clone of the Adafruit layout. Adafruit's document settles two of
these; the other three are what a clone can differ in.

| | Measure | Expect | Got |
|---|---|---|---|
| ☐ | R, SDA → VIN | some kΩ, value unstated by Adafruit | ______ |
| ☐ | R, SCL → VIN | as above | ______ |
| ☐ | R, RST → VIN | fitted — *"pulled high by default"* | ______ |
| ☐ | R, SD0 → VIN | **open** — downstream channels carry no pull-ups | ______ |
| ☐ | C, VIN → GND | **if over 1 µF, reduce C1 to match** (§3, ≤10 µF total on `VSENS`) | ______ |
| ☐ | A0/A1/A2 → GND on the module | **open** — our board ties them, for `0x70` | ______ |

☐ **Module `VIN` goes to `VSENS`, never to permanent 3V3.** The single most
consequential wire on this part.

### U7 — DFR0570 buck

| | Check | Expect | Got |
|---|---|---|---|
| ☐ | Silkscreen at footprint pad 1 | reads **Vin+** | ______ |
| ☐ | R, Vin+ → GND | high, not a short | ______ |
| ☐ | R, Vo+ → GND | high, not a short | ______ |

☐ **Buzz the footprint out before fitting.** A mirrored module footprint puts the
bank on the 3.3 V rail and the Nucleo is downstream of it.

---

## 2. Polarity and orientation — check every line before any power

The three ways `hardware-interface.md` §5 says this node gets destroyed are all in
this table: D9 backwards, C11 backwards, C19 backwards. D10 has already been wired
backwards once, on the FE board.

### Polarised — getting these wrong destroys the part or the board

| | Ref | Part | Mark | Goes to |
|---|---|---|---|---|
| ☐ | **C11** | 100 µF **63 V** | **+** | `24V_PROT` |
| ☐ | **C19** | 100 µF **35 V** | **+** | `24V_PRE` |
| ☐ | **C21** | 22 µF 16 V | **+** | 3.3 V rail — **bend the leads out to 5.08 mm** |
| ☐ | **D9** | P6KE33A, DO-15 | **band = cathode** | `24V_PROT` |
| ☐ | **D10** | 1N4742A 12 V, DO-41 | **band = cathode** | Q2 **source** |
| ☐ | **D11** | 1N4750A 27 V, DO-41 | **band = cathode** | Q3 **gate** |
| ☐ | **D12** | 1N4744A 15 V, DO-41 | **band = cathode** | Q3 **gate** (anode to Q3 source) |

> **C11 and C19 are the pair to be careful with.** Both are 100 µF electrolytics
> and they look identical on the board. **C11 is the 63 V part** — it sits on
> `24V_PROT`, which D9 clamps at 53.3 V. C19 is 35 V and sits on `24V_PRE`, which
> never exceeds ~24 V. Fitting the 35 V part on `24V_PROT` is a part that survives
> the bench and vents in the field.

> **D1–D6 have no polarity.** The CDSOD323-T05LC is a *bidirectional* TVS (§3), so
> the six probe protectors cannot be fitted backwards. Six fewer things to check.

### Three-terminal parts — check each against the datasheet of the part you bought

| | Ref | Part | Pins | Watch for |
|---|---|---|---|---|
| ☐ | Q1 | AO3401A, SOT-23 | 1=G 2=S 3=D | the only SMD three-pin part on the board |
| ☐ | **Q2** | IRF9540N, TO-220 | 1=G 2=D 3=S, **tab = D** | **drain to the supply, source to the load** — backwards from an ideal-diode hookup, on purpose (§5) |
| ☐ | **Q3** | IRF740, TO-220 | 1=G 2=D 3=S, **tab = D** | drain to `24V_PROT`, source to R42 |
| ☐ | **Q4** | BC547, TO-92 | **C B E** from the flat face | **not** E-B-C. A 2N3904 in the same package is E-B-C, and swapping them silently removes the current limit |

☐ **Q4's base and emitter straddle R42** — base to Q3's source, emitter to
`24V_PRE`, collector to Q3's gate. Reversed, the circuit works normally in every
respect except that there is no current limit at all, and you find out during a
fault.

### Both TO-220 tabs are live

☐ **Q2's tab is at `24V_PROT`. Q3's tab is at `24V_PRE`, and so is its heatsink**
unless it is an isolated type. Neither may touch the enclosure, a mounting screw,
or each other.

### Modules and keyed connectors

| | Ref | Orientation cue |
|---|---|---|
| ☐ | U3 | square pad = **VIN**, top of the left column. Not chip pin 1 — that is A0, sixth down |
| ☐ | U7 | square pad = **Vin+** (pad 1) |
| ☐ | J7 | **clip CN10-6 on the ribbon and plug hole 6** — rotated, the socket will not seat |
| ☐ | CN6 | **clip CN6-1 and plug hole 1** — rotated, a full-length socket lands on VIN and will not seat |

---

## 3. Connector pinouts — label both ends of every cable

`hardware-interface.md` §6: four identical 4-pin connectors and four
near-identical cables. A swapped J10/J11 exchanges "inside the house" for
"ambient reference" and the data still looks plausible.

| Connector | Pins | Cable |
|---|---|---|
| **J1–J6** | `V` `DQ` `G` | probes **P0–P5**, 5–10 m, Cat5, DQ twisted with GND |
| **J9** | `V` `SDA` `SCL` `G` | SHT45 #0 — **หัวโรงเรือน** |
| **J10** | `V` `SDA` `SCL` `G` | SHT45 #1 — **ท้ายโรงเรือน** |
| **J11** | `V` `SDA` `SCL` `G` | SHT45 #2 — **นอกโรงเรือน**, needs a radiation shield |
| **J12** | `V` `SDA` `SCL` `G` | SCD41 CO2 — **กลางโรงเรือน**, **`V` and `G` doubled up** |
| **J13** | `TX` `RX` `G` | USB-serial adapter, USART1 |
| **J14** | `+` `−` | 24 V from the charge controller — **the battery terminal, not the LOAD output** |
| **CN6** | 1×8 | to the Nucleo's CN6. Only positions **4 (3V3)**, **6** and **7 (GND)** wired |
| **J7** | 2×20 | ribbon to morpho **CN10** |

☐ J12's `V` and `G` are **two Cat5 conductors each**. One conductor drops 173 mV
at the SCD41's 205 mA burst; doubled, 86 mV.

---

## 4. Assembly order

Lowest first, because a tall part fitted early blocks the iron from a short one.

☐ 1. SMD on the copper side — D1–D6, Q1
☐ 2. Axial resistors, lying flat
☐ 3. Diodes D9, D10, D11, D12 — **polarised, §2**
☐ 4. Ceramic capacitors
☐ 5. Q4 (TO-92), and the 2.54 mm headers — J7, CN6, J13, and the female strips for U3 and U7
☐ 6. Phoenix terminals J1–J6, J9–J12, J14
☐ 7. Electrolytics C11, C19, C21 — **polarised, §2**
☐ 8. Fuse clips and F1 (T2A, 5×20 mm)
☐ 9. Q2 and Q3 (TO-220), Q3 with its heatsink — **tabs live, §2**
☐ 10. The two modules, last, after §1 passes

---

## 5. Before the first volt

☐ Every line of §2 checked, with the board in front of you
☐ `24V_PRE` → GND: **not a short.** A solder bridge at U7's input pins is
invisible on unmasked copper, and it is what the current limiter exists to survive
☐ `24V_PROT` → GND: not a short
☐ `24V_PROT` → `24V_PRE`: **not continuous** — Q3 is between them
☐ Both module footprints buzzed out
☐ F1 fitted, and it is a **T2A** — sized by I²t against Q2's body-diode inrush, not
by amps
☐ Confirm the panel's V_oc and that the charge controller does not pass it through
with the battery disconnected (§7, before step 1)

---

## 6. Power up

| | Step | Expect |
|---|---|---|
| ☐ | 24 V on, nothing else connected | |
| ☐ | `24V_PRE` | **under 28 V**, ~21 V at a 25 V bank |
| ☐ | `24V_PRE` with the bank at its highest (absorb, or 29.2 V forced) | **still under 28 V** — the whole reason the pre-regulator exists |
| ☐ | 3.3 V rail | 3.2–3.4 V. **Over 3.6 V, stop** — CN6-4 and the STM32WL die there |
| ☐ | Whole-board standby current at `24V_PROT`, gate closed | **~0.6 mA** (0.5 module + 0.1 R41). Far above = leak; far below = module not running |
| ☐ | Q3 case during an SCD41 burst | barely warm. Hot = the current limiter is engaging |
| ☐ | I2C scan, all channels closed | **`0x70` and nothing else** — no `0x44`, no `0x62` |

☐ Then the rest of `hardware-interface.md` §7, in order.

**Do not coat the board until §6 passes end to end.** Removing conformal coating
to fix something is worse than any time it saves.

---

## 7. Notes for this board

```
















```
