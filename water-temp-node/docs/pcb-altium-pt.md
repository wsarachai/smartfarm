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

## Where this stands — 2026-09-09

**The schematic is drawn and it verifies**, with one wiring bug and three BOM
fields left. Nothing below is ticked from memory: the three `.SchDoc` files were
read back and a netlist derived from them, so a tick here means the file says so,
not that someone remembers doing it.

| Section | State |
|---|---|
| §1 footprints, all three sheets | **done** — 69 components, every one carrying a `-PT` land as its *current* model |
| §2 the buck sheet's circuit | **done** — the netlist matches §2.2 pin for pin, and `24V_PRE` exists exactly once |
| §3 C1's package | decided — `CERAMIC-P508`, 10 µF, still subject to the §5 audit |
| §3 test points | **undecided** |
| §4 compile, and the three libraries | not started |
| §5 the module pre-fit audit | not started — bench work |
| §6 import to the PCB | not started — `STM32WL_PT.PcbDoc` is still empty |

What the netlist confirms on the buck sheet, because it is the sheet that changed:
Q2 drain on `24V_RAW` and source on `24V_PROT` (the reverse-polarity hookup, not
the ideal-diode one), Q3 drain on `24V_PROT`, its gate on R41 / D11-K / C22 / Q4-C
and its source on D12-A / R42 / Q4-B, Q4's emitter on `24V_PRE`, and U7 with 1+2 on
`24V_PRE`, 5+6 on `V3V3_MCU`, 3/4/7/8 on GND. On the I2C sheet, U3's `VIN` is on
`VSENS` and A0/A1/A2 are all on GND — the two things §3 of the spec delta calls not
optional.

### The one bug — R23 pulls `RESET` **down**, and it is on both boards

`I2C-sensors.SchDoc` wires **R23 pin 1 to a GND port** and pin 2 to `U3.RESET`. The
spec wants the other rail: `hardware-interface.md` §4a lists R23 as the `RESET`
**pull-up to `VSENS`**, and its *five ways this goes wrong* names a held `RESET` as
a **total I2C blackout, including CO2** — every device on this node is behind the
mux, so there is nothing upstream left working to reassure you.

It will not even fail cleanly here. The breakout carries its own `RESET` pull-up
(Adafruit p.6, and this module is a clone of that layout), so the two resistors
divide to about **1.65 V** — above the TCA9548A's V_IL max of 0.99 V and below its
V_IH min of 2.31 V. That is the undefined band: an intermittent mux rather than a
dead one, which is the harder of the two to diagnose.

**Inherited, not introduced.** `STM32WL_FE/I2C-sensors.SchDoc` carries the same
wire at the same coordinates. The FE board was never ordered so nothing is lost —
but fix both, or the next board copied from either one inherits it again.

☑ R23 pin 1 moved off the GND port and onto `VSENS` — `STM32WL_PT`, 2026-09-09.
The GND port at the end of that wire was replaced by a `VSENS` port of the same
**Bar** style as the sheet's other three, so the fix carries the right net name and
the right symbol; the sheet is down to three GND ports from four.
☐ The same fix in `STM32WL_FE` — **not done**, that file is untouched since
2026-09-06 and still has the GND port at the same coordinate.

### Three fields that would have reached the BOM wrong — fixed 2026-09-09

The board has **no silkscreen at all**. `build-sheet-proto.md` is the only thing on
site that says which part goes where, and it is generated from these fields, which
is why three stale strings were worth a pass of their own.

☑ **D9** `Comment` `SMBJ33A` → `P6KE33A`. The footprint and its own description had
said P6KE33A all along; only the field the BOM prints had been missed.
☑ **D12** `Value` `27V 1W` → `15V 1W`. It had been copied from D11 when the part
was placed, against a `Comment` that was already right (`1N4744A`). A 27 V part in
D12's place is not a gate–source clamp at all.
☑ **R23** `Comment` `220R` → `10K`, matching its `Value`.

---

## 0. There is no blocker any more

Both modules were measured on **2026-09-08** and nothing in this document waits on
a part now. The one item that was left — U7's eight-pin symbol — was drawn on
2026-09-08 into `STM32WL_PT.SchLib`, which is now `[Document20]` of the project.

| | Symbol | Footprint |
|---|---|---|
| **U3** TCA9548A breakout | **have it** — reuse `TCA9548APWR.SchLib` | **have it** — `MOD-TCA9548A` |
| **U7** DFR0570 | **have it** — `STM32WL_PT.SchLib`, 8 pins named `Vin+`/`GND`/`Vo+` | **have it** — `MOD-DFR0570` |

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

☑ All of the above changed — verified by reading the three `.SchDoc` files back:
30 axial resistors, 5 `CERAMIC-P508` (C1, C2, C8, C10 and the new C22), 2
`RADIAL-D8-P35`, 1 `RADIAL-D5-P20`, `DO15-P1270`, 3 `DO41-P762`, 2
`TO220-VERT-STAG`, 1 `TO92-INLINE-P254`, 6+4+1 Phoenix, `HDR2X20-BOX-PT`,
`HDR1X8-P254-PT`, `HDR1X3-P254-PT`, `FUSEHOLDER-5X20-P226`, both module lands, and
the six `CDSOD323_BRN-M` plus `SOT23-3-M` left deliberately SMD. In every case the
`-PT` land is the **current** model, not merely an attached one.
☐ `Tools » Footprint Manager` shows **no component with a missing model**

---

## 2. `Buck-regulator.SchDoc` — the only sheet whose circuit changes

Everything in this section comes from `hardware-interface-proto.md` §4.

### 2.1 Change three parts in place

☑ **Q2** → `IRF9540N`, generic `MOSFET-P` symbol. Keep the orientation: **drain to
the supply, source to the load** — the reverse-polarity hookup of
`hardware-interface.md` §5, which is backwards from an ideal-diode hookup on
purpose. R38 and D10 stay exactly as they are.
☑ **D9** → `P6KE33A`, footprint `DO15-P1270`, anode on GND and cathode on
`24V_PROT`. **Its `Comment` field is still `SMBJ33A`** — see the BOM list above.
☑ **D10** → `1N4742A`, 12 V. Two leads instead of three, so the SOT-23 pin-numbering
trap that caught this part once cannot recur — but the band is still the cathode
and it still goes to **Q2's source**.

### 2.2 Add the pre-regulator block, and the net `24V_PRE`

Seven new parts between `24V_PROT` and the module. Draw it as its own block on the
sheet, not squeezed into the existing power chain — it is a distinct stage.

☑ **Q3** — generic `MOSFET-N`, `TO220-VERT-STAG`. Drain to `24V_PROT`, source to R42.
☑ **R41** — 22 kΩ, `AXIAL-R-P1016`, from `24V_PROT` to Q3's gate.
☑ **D11** — 27 V Zener (1N4750A), `DO41-P762`, **cathode to Q3's gate**, anode to GND.
☑ **C22** — 100 nF, `CERAMIC-P508`, Q3's gate to GND.
☑ **D12** — 15 V Zener (1N4744A), `DO41-P762`, **cathode to Q3's gate, anode to Q3's
source**. Gate–source clamp.
☑ **R42** — 4.7 Ω, `AXIAL-R-P1016`, Q3's source to `24V_PRE`.
☑ **Q4** — generic `NPN`, `TO92-INLINE-P254`. **Base to Q3's source, emitter to
`24V_PRE`, collector to Q3's gate.** Base and emitter straddle R42; get this the
wrong way round and there is no current limit at all. **Drawn right** — the
symbol's pins are named C/B/E, and the netlist puts C on the gate net, B on Q3's
source and E on `24V_PRE`.
☑ **`24V_PRE`** added as a power port, the same style as `24V_PROT` — this project
carries all cross-sheet connectivity on power ports and has no net labels
(`pcb-altium.md` §0). Do not introduce the first net label here.

### 2.3 Move C19

☑ **C19** — value to **100 µF / 35 V**, and move its `+` from `24V_PROT` to
**`24V_PRE`**. It is the module's local input bulk now, and it is what gives the
current limiter a smooth DC load instead of the buck's 500 kHz input pulses.
☑ **C11 does not move.** It stays on `24V_PROT` at **≥63 V**. Its job is damping the
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

☑ **C1's package.** Set to `CERAMIC-P508` at 10 µF — which spends §3's **10 µF**
`VSENS` ceiling in full. So if the §5 audit finds more than a trace of decoupling
on the mux module, **C1 comes down**, and that is a change to make before §6
rather than after.
☑ **Test points.** Decided with the spec: **none this revision**. §6 of the shared
spec says so, the parts list has no `TP` refs, and on a bare-copper board with no
soldermask every pad is a test point anyway. `TESTPOINT.SchLib` was detached from
the project on 2026-09-09.

---

## 4. Compile, and clear the project

☑ **`Project » Compile PCB Project STM32WL_PT.PrjPcb`** — 2026-09-09: **zero
errors, one warning**, and the warning is the expected one:

> `[Warning] I2C-sensors.SchDoc  Net NetR23_2 has no driving source (Pin R23-2, Pin U3-3)`

`U3.RESET` is an **Input** pin (the TI symbol leaves `ELECTRICAL` at its default),
and the only other pin on its net is R23 pin 2, which is **Passive**. Altium's
connectivity does not cross a resistor, so `VSENS` on R23's far side is not a
driver *of this net* — a pull-up feeding an input always looks like this. It is not
a symptom of the R23 fix above; the same net, and the same warning, existed when
R23 went to GND.

It is also the reason this is the **only** warning: A0/A1/A2 are Input pins too,
but they sit on GND, and a power port counts as a driver.

☐ Suppress it at the pin — `Place » Directives » Generic No ERC` on `U3` pin 3 —
rather than by lowering *Nets with no driving source* in `Project » Project
Options » Error Reporting`, which is global and would hide a genuinely undriven
input somewhere else. `FrontEnd-signals.SchDoc` already carries 26 No-ERC
directives; `I2C-sensors.SchDoc` carries none, and this would be its first.
☐ Check the compile output for **floating power ports**. This project carries all
connectivity on power ports, so a mistyped `24V_PRE` does not error — it silently
creates a second, unconnected net with a similar name. Look at the Net list and
confirm there is exactly one `24V_PRE`.
◪ **Remove the three libraries the proto no longer uses** — `DMP6023LE-13`,
`SMBJ33A`, `BZX84C12`. They were kept attached deliberately until now, because the
copied sheets still carried those symbols. They come off *after* the parts are
swapped, not before (`pcb-altium.md` §2.1a). **2026-09-09: two of the three are
gone** (`DMP6023LE-13` and `SMBJ33A`, both halves each), and `TESTPOINT.SchLib`
went with them, settling §3's second question in the spec's favour. **`BZX84C12`
is still attached** — it is two files under one folder name,
`diode-nc_pin.SchLib` and `SOT-23.PcbLib`, which is what makes it the easy one to
miss.
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
☑ **`STM32WL_PT.SchLib` has the symbol** — `MOD-DFR0570`, eight pins, 1,2 = Vin+;
5,6 = Vo+; 3,4,7,8 = GND. U3 keeps the `TCA9548APWR` symbol it already has.
☑ Both footprints live in `STM32WL_PT.PcbLib`, built by the script — not in
`..\Lib\`, which is shared with `STM32WL_FE`.
☑ **U3** — placed and wired. **`VIN` (chip pin 24) to `VSENS`, never to permanent
3V3**; A0/A1/A2 (chip pins 1, 2, 21) to GND for address `0x70`; R23's 10 kΩ to
`RESET` (chip pin 3). Watch the SD/SC column flip noted above.
☑ **U7** — placed, footprint `MOD-DFR0570`. Pins **1+2 from `24V_PRE`**, pins
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

- `pcb-altium-pt.th.md` — the Thai mirror. The two documents that are read at a
  bench rather than at a desk (`pcb-home-etch` and `build-sheet-proto`) already
  have theirs; this one is a desk document, which is why it is last.
