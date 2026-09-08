# Editing the schematic for `STM32WL_PT`

The work order that turns the three sheets copied from `STM32WL_FE` into the
self-etched board. Companion to [`pcb-home-etch.md`](pcb-home-etch.md), which
covers everything after the artwork exists, and to
[`hardware-interface-proto.md`](hardware-interface-proto.md), which is where every
value below comes from.

**This document decides nothing.** If it disagrees with
`hardware-interface-proto.md`, that one is right and this is a bug.

**The sheets are already the proto's own copies.** `STM32WL_PT/` holds
`FrontEnd-signals.SchDoc`, `I2C-sensors.SchDoc` and `Buck-regulator.SchDoc`, and
`STM32WL_PT.PrjPcb` contains the string `STM32WL_FE` zero times — see §2.1a of
[`pcb-altium.md`](pcb-altium.md) for why that mattered. Editing here cannot damage
the FE board.

---

## 0. There is no blocker any more

Both modules were measured on **2026-09-08** and nothing in this document waits on
a part now. One item is left and it is small: U7 needs a schematic symbol drawn,
which is eight pins and takes ten minutes.

| | Symbol | Footprint |
|---|---|---|
| **U3** TCA9548A breakout | **have it** — reuse `TCA9548APWR.SchLib` | **have it** — `MOD-TCA9548A` |
| **U7** DFR0570 | **draw it** — 8 pins, §5 gives the numbering | **have it** — `MOD-DFR0570` |

**U3 is not blocked at all**, and the reason is worth keeping: a breakout board
brings the chip's pins out to a header and labels them with the chip's own signal
names, so the *schematic* symbol is the chip's symbol — already attached to this
project at `..\Lib\TCA9548APWR\TCA9548APWR.SchLib`. That left only the header
geometry, which was measured on 2026-09-08. Both halves are done; see *The module
as measured* below.

**U7 was the dangerous one**, because DFRobot publish no pin drawing and a wrong
pin order puts the bank voltage on the 3.3 V rail with the Nucleo downstream —
`hardware-interface.md` §5 makes the same point about the TSR. It was settled by
measurement rather than by inference, and the measurements then landed on a 0.1 in
grid to within 0.4 mm, which is a second, independent check that the reading was
right. See *The buck module as measured* below.

### The TCA9548A pin functions, as the authority for both

From TI **SCPS207H** (May 2012, revised September 2024), Table 4-1, PW/DGS 24-pin.
Use it to check the module's silkscreen labels rather than trusting them:

| Pin | Name | Pin | Name | Pin | Name |
|---:|---|---:|---|---:|---|
| 1 | A0 | 9 | SC2 | 17 | SD6 |
| 2 | A1 | 10 | SD3 | 18 | SC6 |
| 3 | **RESET** | 11 | SC3 | 19 | SD7 |
| 4 | SD0 | 12 | GND | 20 | SC7 |
| 5 | SC0 | 13 | SD4 | 21 | **A2** |
| 6 | SD1 | 14 | SC4 | 22 | SCL |
| 7 | SC1 | 15 | SD5 | 23 | SDA |
| 8 | SD2 | 16 | SC5 | 24 | VCC |

Three things in that table are load-bearing here:

- **`RESET` is active-low and the datasheet says to pull it up if unused** — it
  names no internal pull-up. That is the citation behind §4a of the shared spec
  calling R23 "not optional", and it is why a module with no onboard `RESET`
  pull-up would leave the mux held in reset by nothing at all.
- **A0, A1 and A2 are not adjacent.** A0 and A1 are pins 1–2; **A2 is pin 21**,
  across the package between SC7 and SCL. On a breakout they may well be brought
  out in three different places. All three go to GND for address `0x70`.
- **V_CC is 1.65–5.5 V**, so the module is happy on `VSENS` at 3.3 V.

### The module as measured — SKU-0260-1

22.0 × 31.0 mm, two rows of twelve on 2.54 mm pitch, **rows 17.78 mm apart** centre
to centre. Built as `MOD-TCA9548A` by `MakeFootprintsPT.pas` §13.

| pos | left | chip pin | right | chip pin |
|---:|---|---:|---|---:|
| 1 | **VIN** | 24 | SC7 | 20 |
| 2 | GND | 12 | SD7 | 19 |
| 3 | SDA | 23 | SC6 | 18 |
| 4 | SCL | 22 | SD6 | 17 |
| 5 | RST | 3 | SC5 | 16 |
| 6 | A0 | 1 | SD5 | 15 |
| 7 | A1 | 2 | SC4 | 14 |
| 8 | A2 | 21 | SD4 | 13 |
| 9 | **SD0** | 4 | **SC3** | 11 |
| 10 | **SC0** | 5 | **SD3** | 10 |
| 11 | **SD1** | 6 | **SC2** | 9 |
| 12 | **SC1** | 7 | **SD2** | 8 |

**Independently confirmed.** This pin order is exactly the Adafruit TCA9548A
breakout's, from `adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout.pdf` p.6 —
same twelve on the left, same twelve on the right, same order. SKU-0260-1 is a
clone of that layout, so the pinout now rests on a measurement *and* a published
drawing that agree. Adafruit's board also carries **two mounting holes on the
vertical centreline**; check whether the clone has them before routing under it.

**Two things Adafruit's document settles that the pre-fit audit was going to have
to discover:**

- **The downstream channels have no pull-ups fitted** — *"These pins do not have
  any pullups installed"* (p.8). That is exactly what this design needs: R17–R22
  and R39/R40 are then the only pull-ups on those lines, at the 2.2 kΩ §3 sizes
  them at for 5 m of cable, with nothing in parallel to stiffen them.
- **`RST` is pulled high on the board** — *"Pulled high by default"* (p.6). R23's
  10 kΩ is therefore in parallel with an onboard pull-up rather than alone. Still
  fit R23: the value lands somewhere around 5 kΩ, which is harmless, and a clone
  that omits the onboard resistor would otherwise leave `RESET` floating on an
  active-low pin with no internal pull-up (TI SCPS207H).

**Still measure before fitting:** the upstream SDA/SCL pull-up values and the
decoupling capacitor. Adafruit do not state either on these pages, and the clone
is a clone — the pin *order* matching does not mean the *components* match.

**The pads are named by chip pin, not by row position, and that is the whole
reason this footprint is hand-written rather than a generic 2×12 header.** U3's
symbol is the chip's, numbered 1–24 per SCPS207H. Number the pads 1–24 down the
columns instead and every net lands on the wrong pin — with a netlist that is
perfectly self-consistent and no DRC error anywhere.

**The square pad is VIN, not chip pin 1.** Chip pin 1 is A0, sixth down the left
column. The square pad has to mean *this corner*, because on a board with no
silkscreen it is the only orientation mark that survives etching.

Two things that matter once you are routing:

- **The SD/SC order flips between the columns.** Left runs SD then SC; right runs
  SC then SD. This is where a channel's SDA and SCL get transposed, and the
  symptom — one sensor silent while the others are fine — looks like a bad cable,
  not like a wiring error.
- **Channels 0–3, the only four used, are all in the bottom third**: 0 and 1 on the
  left, 2 and 3 on the right. Channels 4–7 sit unused at the top of the right
  column. The upstream group (VIN, GND, SDA, SCL, RST, A0, A1, A2) is contiguous at
  the top of the left column, so it needs no routing across the module.

### The buck module as measured — DFR0570

22.5 × 17.0 mm, **eight holes on a 0.1 in grid**, two columns 17.78 mm apart
running along the 22.5 mm dimension. Built as `MOD-DFR0570` by
`MakeFootprintsPT.pas` §14.

| pad | net | X | Y | | pad | net | X | Y |
|---:|---|---:|---:|---|---:|---|---:|---:|
| **1** | **Vin+** | −8.89 | +6.35 | | 5 | Vo+ | +8.89 | +6.35 |
| 2 | Vin+ | −8.89 | +3.81 | | 6 | Vo+ | +8.89 | +3.81 |
| 3 | GND | −8.89 | −3.81 | | 7 | GND | +8.89 | −3.81 |
| 4 | GND | −8.89 | −6.35 | | 8 | GND | +8.89 | −6.35 |

**Why the grid is the evidence, not the tape measure.** Four independent
measurements each landed within 0.4 mm of a 0.1 in multiple — 17.5→17.78,
12.5→12.70, 8.0→7.62, 2.54→2.54 — and the outer and inner spans differ by
**exactly twice the pair pitch** (12.70 − 7.62 = 5.08 = 2 × 2.54). Four
coincidences do not happen. It also matches DFRobot's own *"2.54 mm pin
plug-in"*, so the module seats on ordinary female header strip and lifts out
again, exactly like U3.

**Eight holes, four nets.** Vin+ and Vo+ are doubled and GND is quadrupled —
that is the module's 3 A rating, not ours. At 46 mA one of each would carry it,
and all eight still get soldered.

**Wire 1+2 to `24V_PRE`, 5+6 to the 3.3 V rail, 3+4+7+8 to GND.** The symbol
needs eight pins numbered to match. **Do not collapse it to three pins** — the
footprint and symbol then disagree about how many pads exist, and Altium will not
say which one it believes.

**Pad 1 is Vin+ and is square.** A mirrored footprint here puts the bank on the
3.3 V rail; §7 buzzes it out before the module is fitted.

Everything else uses a **generic symbol** from `Miscellaneous Devices.IntLib` with
the right footprint attached — which is what the FE board already does for Q2. No
new symbol library is needed for them.

---

## 1. Set every footprint to the `-PT` set — all three sheets

Do this first and in bulk. The `-PT` footprints are the ones
`MakeFootprintsPT.pas` built, and they exist because a hand-drilled hole needs a
bigger pad than a fab-house one (`pcb-home-etch.md` Stage 1).

**Use `Tools » Parameter Manager`, not the properties dialog part by part.** Forty
components, and the failure mode of doing it by hand is missing one — which shows
up as a part that will not fit, after the board is etched.

| Refs | New footprint | Count |
|---|---|---|
| R1, R2, R3–R8, R9–R14, R15, R16, R17–R22, R23, R24, R25, R38, R39, R40 | `AXIAL-R-P1016` | 30 |
| C2, C8, C10 | `CERAMIC-P508` | 3 |
| C1 | `CERAMIC-P508` **or** `RADIAL-D5-P20` — see §3 | 1 |
| C11, C19 | `RADIAL-D8-P35` | 2 |
| C21 | `RADIAL-D5-P20` | 1 |
| D9 | `DO15-P1270` | 1 |
| D10 | `DO41-P762` | 1 |
| Q1 | `SOT23-3-M` — unchanged, stays SMD | 1 |
| Q2 | `TO220-VERT-STAG` | 1 |
| J1–J6 | `PHX-MC15-3-G-35-PT` | 6 |
| J9–J12 | `PHX-MC15-4-G-35-PT` | 4 |
| J14 | `PHX-MC15-2-G-35-PT` | 1 |
| J7 | `HDR2X20-BOX-PT` | 1 |
| J13 | `HDR1X3-P254-PT` | 1 |
| CN6 | `HDR1X8-P254-PT` | 1 |
| F1 | `FUSEHOLDER-5X20-P226` | 1 |
| D1–D6 | **unchanged** — `CDSOD323_BRN-M` from the vendor library | 6 |
| U3 | `MOD-TCA9548A` | 1 |
| U7 | `MOD-DFR0570` | 1 |

☐ All of the above changed
☐ `Tools » Footprint Manager` shows **no component with a missing model**

---

## 2. `Buck-regulator.SchDoc` — the only sheet whose circuit changes

Everything in this section comes from `hardware-interface-proto.md` §4.

### 2.1 Change three parts in place

☐ **Q2** → `IRF9540N`, generic `MOSFET-P` symbol. Keep the orientation: **drain to
the supply, source to the load** — the reverse-polarity hookup of
`hardware-interface.md` §5, which is backwards from an ideal-diode hookup on
purpose. R38 and D10 stay exactly as they are.
☐ **D9** → `P6KE33A`. Still unidirectional, still **cathode to `24V_PROT`**.
☐ **D10** → `1N4742A`, 12 V. Two leads instead of three, so the SOT-23 pin-numbering
trap that caught this part once cannot recur — but the band is still the cathode
and it still goes to **Q2's source**.

### 2.2 Add the pre-regulator block, and the net `24V_PRE`

Seven new parts between `24V_PROT` and the module. Draw it as its own block on the
sheet, not squeezed into the existing power chain — it is a distinct stage.

☐ **Q3** — generic `MOSFET-N`, `TO220-VERT-STAG`. Drain to `24V_PROT`, source to R42.
☐ **R41** — 22 kΩ, `AXIAL-R-P1016`, from `24V_PROT` to Q3's gate.
☐ **D11** — 27 V Zener (1N4750A), `DO41-P762`, **cathode to Q3's gate**, anode to GND.
☐ **C22** — 100 nF, `CERAMIC-P508`, Q3's gate to GND.
☐ **D12** — 15 V Zener (1N4744A), `DO41-P762`, **cathode to Q3's gate, anode to Q3's
source**. Gate–source clamp.
☐ **R42** — 4.7 Ω, `AXIAL-R-P1016`, Q3's source to `24V_PRE`.
☐ **Q4** — generic `NPN`, `TO92-INLINE-P254`. **Base to Q3's source, emitter to
`24V_PRE`, collector to Q3's gate.** Base and emitter straddle R42; get this the
wrong way round and there is no current limit at all.
☐ **`24V_PRE`** added as a power port, the same style as `24V_PROT` — this project
carries all cross-sheet connectivity on power ports and has no net labels
(`pcb-altium.md` §0). Do not introduce the first net label here.

### 2.3 Move C19

☐ **C19** — value to **100 µF / 35 V**, and move its `+` from `24V_PROT` to
**`24V_PRE`**. It is the module's local input bulk now, and it is what gives the
current limiter a smooth DC load instead of the buck's 500 kHz input pulses.
☐ **C11 does not move.** It stays on `24V_PROT` at **≥63 V**. Its job is damping the
input cable's resonance from inside D9's clamp, and none of that changed.

> The 63 V / 35 V split is the easiest thing on this sheet to get backwards, and
> both are 100 µF electrolytics that look identical on the drawing. `24V_PRE` never
> exceeds ~24 V; `24V_PROT` sees D9's 53.3 V clamp.

---

## 3. `FrontEnd-signals.SchDoc` and `I2C-sensors.SchDoc`

**No circuit changes at all** — only the footprints of §1. Every value, every
pull-up and the whole connector contract are unchanged
(`hardware-interface-proto.md` §1).

Two things to confirm rather than change:

☐ **C1's package.** §3 of the shared spec caps total `VSENS` bulk at **10 µF**, and
the mux module brings its own decoupling. Leave C1's value open until the module
has been measured (§5 below), then pick the package to suit.
☐ **Test points.** §6 of the shared spec says this revision has none, and the parts
list has no `TP` refs — but `TESTPOINT.SchLib` is attached to the project and the
FE audit mentions test pads. Decide which is true before layout; on a bare-copper
board with no soldermask, every pad is a test point anyway.

---

## 4. Compile, and clear the project

☐ **`Project » Compile PCB Project STM32WL_PT.PrjPcb`** — zero errors.
☐ Check the compile output for **floating power ports**. This project carries all
connectivity on power ports, so a mistyped `24V_PRE` does not error — it silently
creates a second, unconnected net with a similar name. Look at the Net list and
confirm there is exactly one `24V_PRE`.
☐ **Remove the three libraries the proto no longer uses** — `DMP6023LE-13`,
`SMBJ33A`, `BZX84C12`. They were kept attached deliberately until now, because the
copied sheets still carried those symbols. They come off *after* the parts are
swapped, not before (`pcb-altium.md` §2.1a).
☐ **`TCA9548APWR` stays**, against the earlier plan: the module reuses its symbol
(§0). Only the footprint changes, and the footprint is the part that is still
missing.
☐ `AO3401A` and `CDSOD323-T05LC` stay — Q1 and D1–D6 are unchanged.

---

## 5. The modules

Both were measured on 2026-09-08 and both footprints exist. What is left:
☐ **Run the TCA9548A pre-fit audit** from [`build-sheet-proto.md`](build-sheet-proto.md)
*before* anything is soldered: the onboard SDA/SCL/RST pull-up values, and the
decoupling capacitor. If that capacitor is over 1 µF it comes out of C1's budget
(§3 above, and `hardware-interface-proto.md` §3).
☐ **`STM32WL_PT.SchLib` needs one symbol** — `MOD-DFR0570`, **eight pins**
numbered as in §0: 1,2 = Vin+; 5,6 = Vo+; 3,4,7,8 = GND. U3 keeps the
`TCA9548APWR` symbol it already has.
☐ Both footprints live in `STM32WL_PT.PcbLib`, built by the script — not in
`..\Lib\`, which is shared with `STM32WL_FE`.
☐ **U3** — already placeable. **`VIN` (chip pin 24) to `VSENS`, never to permanent
3V3**; A0/A1/A2 (chip pins 1, 2, 21) to GND for address `0x70`; R23's 10 kΩ to
`RESET` (chip pin 3). Watch the SD/SC column flip noted above.
☐ **U7** — place, footprint `MOD-DFR0570`. Pins **1+2 from `24V_PRE`**, pins
**5+6** to the 3.3 V rail feeding CN6 and Q1's source, pins **3,4,7,8** to GND.
☐ **Buzz both footprints out before soldering.** A footprint mirrored or rotated on
a hand-drilled board puts 24 V on an output pin.

---

## 6. Across to the PCB

☐ `Design » Import Changes From STM32WL_PT.PrjPcb`
☐ Every component arrives with a `-PT` footprint or one of the two module
footprints. Nothing arrives with an FE-board land.
☐ Then [`pcb-home-etch.md`](pcb-home-etch.md) Stage 1 for the layout rules —
bottom layer is the real board, GND poured on both faces, nothing routed between
2.54 mm pins, and vias counted as you go.

---

## Not yet written

- `pcb-altium-pt.th.md` — the Thai mirror.
- [`build-sheet-proto.md`](build-sheet-proto.md) — §5 above depends on its
  TCA9548A pre-fit audit.
