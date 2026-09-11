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

## Where this stands — 2026-09-10

**The schematic is drawn and it verifies**, and the board has rules, artwork and
all 69 parts on it. Nothing below is ticked from memory: the three `.SchDoc`
files, `STM32WL_PT.PcbDoc` and both PCB ECO logs were read back and the netlist
derived from them, so a tick here means the file says so, not that someone
remembers doing it.

**Re-audited 2026-09-10, after the afternoon's schematic edits.** Everything the
morning's entry claimed still holds — the six design rules carry the right values
in the file, the origin is at 20/20 mm, FID1–3 and both scale bars are there, no
rooms survive, and Bug 1's fix reads back as three GND ports and four Bar ports on
the I2C sheet. Two things changed since: the three Zener `Comment` fields now
carry their voltage as well as their part number (`1N4742A 12V`, `1N4744A 15V`,
`1N4750A 27V`), which is worth having on a board with no silkscreen — and **Q1
picked up a new bug**, below.

| Section | State |
|---|---|
| §1 footprints, all three sheets | **done** — 69 components, every one carrying a `-PT` land as its *current* model |
| §2 the buck sheet's circuit | **done** — the netlist matches §2.2 pin for pin, and `24V_PRE` exists exactly once |
| §3 C1's package | decided — `CERAMIC-P508`, 10 µF, still subject to the §5 audit |
| §3 test points | decided — none this revision |
| §4 compile, and the three libraries | **done** — zero errors, zero warnings |
| §5 the module pre-fit audit | **next** — bench work, and C1 depends on it |
| **Q1's symbol — Bug 2** | **closed** 2026-09-10 14:56 — pins on `AO3401A`'s numbering, land back to `SOT23-3-M`, verified out of the saved board |
| §6 import to the PCB | **done** — 69 of 69, board artwork already placed |
| Layout, `pcb-home-etch.md` Stage 1 | **placed and DRC-clean** 2026-09-10 22:10. Routing next |
| DRC on the placed board | **clean** — only unrouted nets and F1's documented reamed holes remain |
| Symbol pin numbering, all parts | **swept clean** 2026-09-11 — Bugs 2 and 3 were the only three occurrences |

What the netlist confirms on the buck sheet, because it is the sheet that changed:
Q2 drain on `24V_RAW` and source on `24V_PROT` (the reverse-polarity hookup, not
the ideal-diode one), Q3 drain on `24V_PROT`, its gate on R41 / D11-K / C22 / Q4-C
and its source on D12-A / R42 / Q4-B, Q4's emitter on `24V_PRE`, and U7 with 1+2 on
`24V_PRE`, 5+6 on `V3V3_MCU`, 3/4/7/8 on GND. On the I2C sheet, U3's `VIN` is on
`VSENS` and A0/A1/A2 are all on GND — the two things §3 of the spec delta calls not
optional.

### Picking this up again

The schematic is finished and committed. The board is open work, and it lives in
files git does not track — see the warning at the end of this section.

**Done, and verified by reading `STM32WL_PT.PcbDoc` back:**

- All 69 components imported with the right land.
- The board artwork is placed (outline, origin, three fiducials, `100.0 mm` scale
  bar, `TOP`/`BOT` copper markers) — `DrawArtworkPT` has already run. Running it a
  second time would duplicate all of it; `CheckBoardPT` is the safe way to ask.
- **Six design rules set**, values confirmed out of the file (Altium stores them in
  mil): `Width` 0.5/1.0 mm at priority 2 scoped `All`; **`W_POWER` 3.0/6.0 mm at
  priority 1** scoped `InNet('24V_PROT') or InNet('24V_PRE') or InNet('GND')` —
  the priority is what makes the 3 mm rule win, and a `W_POWER` sitting below
  `Width` silently does nothing; `Clearance` 0.5 mm; `RoutingVias` 2.0 mm pad /
  0.8 mm hole; `HoleSize` 0.8–1.3 mm; `MinimumAnnularRing` **0.45 mm**.
- The three sheet rooms Altium added at import have been deleted, because §6's
  floorplan is edge-driven and crosses every sheet boundary.

**`MinimumAnnularRing` = 0.45 mm is derived, not quoted.** It is the narrowest ring
on the board — the 1.9 mm pad on a 1.0 mm hole that Stage 1 sizes for 2.54 mm
pitch, so (1.9 − 1.0) / 2. Everything else is wider (Phoenix 0.55, general 0.70,
via 0.60). Altium's own default is 0.05 mm, which catches nothing.

**Bug 2 is closed**, so routing is unblocked — the ratsnest at Q1 points at the
part's own pads again.

**Next action: placement**, in the order §6 of
[`hardware-interface-proto.md`](hardware-interface-proto.md) fixes — edge
connectors first, then the 24 V corner in the physical sequence
J14 → F1 → Q2 → D9 → C11, then the modules, then the rest.

**`hardware/scripts/PlacePartsPT.pas` does the first pass.** `CheckPlacementPT`
reports what it can see, `PlaceFloorplanPT` puts all 69 parts down and **locks
the sixteen whose position is a specification** — J1–J7, J9–J14, CN6, U3, U7 —
and `DrawKeepoutsPT` draws the three no-copper rectangles (U3, U7, Q3's
heatsink). It sets absolute positions rather than nudging, so running it twice
is harmless; `DrawKeepoutsPT` is the one that is not. **The other 53 land on a
starting floorplan, not a specification** — they are grouped with what they
belong to and cleared against the 0.5 mm rule, and they are meant to be dragged
while routing. The script's own comments carry the three groups worth checking
by eye first: the 24 V chain's order, C19's distance to U7 (**7.6 mm pad to pad,
which is the geometric floor** — an 8 mm can beside a module whose outline
reaches 2.36 mm past its pad column), and the `VBAT_SENSE` divider.

**Rotation there is placement, not polarity.** Everything lands at 0° or 90°
because that is what makes the ratsnest readable. D9, D10, D11, D12, C11 and C19
are polarised and [`build-sheet-proto.md`](build-sheet-proto.md) §2 is what says
which way round they go; expect to flip several while routing.

Three things to carry into it that are not obvious from the floorplan sketch:

- **C19 belongs beside U7, not in the 24 V corner.** The ASCII sketch groups it
  with C11 out of history — it moved to `24V_PRE` and is now the module's input
  bulk, which §5 wants within 5 mm of the module pins. The prose is right and the
  picture is stale.
- **J1–J6 run P0→P5 along the bottom edge and the order is load-bearing** — it is
  `DS_PROBE_BUSES` and the dashboard metric names. Swapping two silently relabels
  which end of the greenhouse a reading came from.
- **J13 is not in the sketch at all.** It goes on the left edge near J7, whose pins
  35/37 carry its `DBG_TX`/`DBG_RX`.

Edge budget, checked against the real footprints: bottom needs ~84 mm of 160
(J1–J6 + J14), right ~64 of 120 (J9–J12), left ~77 of 120 (J7's 57 mm shroud plus
CN6). Every edge fits with room to spare, which is what §6's "~256 mm of edge"
was counting.

### What the first DRC found — 2026-09-10

`PlaceFloorplanPT` and `DrawKeepoutsPT` both ran clean, and the saved board
checks out: **all 69 components at exactly the scripted coordinates and
rotations**, exactly the **16 intended locks**, and **12 keep-out tracks** forming
three closed rectangles with no duplicates. Read back out of `Components6` and
`Tracks6`, not from the dialogs that reported it.

The DRC then found two things worth writing down.

**1. Two ComponentClearance violations — Q2 against R38, and Q2 against D9.**
Fixed by moving `R38` to (112, 33) and `D9` to (104, 18); the tightest pair on
the board is now D10–Q2 at 0.69 mm against a 10 mil rule.

The cause is worth more than the fix. `TO220-VERT-STAG`'s silk body runs from
**+1.60 to +6.30 above the pad row**, so a TO-220 placed at y=18 reaches y=24.4
and is **10.4 × 9.6 mm**, not the 7.5 × 4.4 its three pads suggest. The geometric
check that cleared this floorplan before it was ever run carried a **hand-typed**
table of footprint boxes, and its TO-220 entry was wrong in exactly that way. It
passed, and Altium disagreed.

That is the same failure as Bug 2 one level up: a model of a part, typed out by
hand, that looks right and that nothing contradicts. So the check no longer types
anything — `hardware/scripts/check_floorplan.py` **derives every extent by reading
the actual `AddPadPT` / `AddRowPT` / `AddSilkBoxPT` calls out of
`MakeFootprintsPT.pas`**. Against the saved board it reproduces Altium's DRC
exactly: the same two violations, no false positives and no misses. Run it after
moving anything in `PlacePartsPT.pas`.

**2. Eight MinWidthStubTrack violations, all `Actual Width = 0.4mm, Target Width
= 0.5mm`** — the scale bar and its three ticks, on both copper layers. They are
artwork, not net copper, but they sit on copper and the `Width` rule polices them
like anything else. **GATE 1 asks for a clean DRC, and a board carrying eight
known-harmless violations cannot honestly pass it** — that is how the ninth,
which is not harmless, gets waved through. `SetupBoardPT.pas` now draws them at
0.5 mm. On *this* board, select the eight tracks and set their width to 0.5 mm.

### The second DRC — and the one that would have cost the board

The clearance fix and the re-run landed: `TComponentClearanceViolation` is gone
from the saved file and R38/D9 read back at their new positions. **The DRC was
not clear, though** — the 20:56 file carries **305 violation records**. Most are
noise for a board of this kind, but one is not.

**`RADIAL-D5-P20` is a short circuit.** `Make_RadialCan` takes the pitch as a
parameter and hard-codes the pad at **2.2 mm**, so C21's **2.0 mm** land puts two
2.2 mm pads 2.0 mm apart — **0.20 mm of overlap**. C21 is the 22 µF on the 3.3 V
rail and its two pads are the rail and ground, so the land itself **shorts 3.3 V
to GND, etched into the board before a single part is fitted**. `RADIAL-D8-P35`
gets away with the same hard-coded pad because 3.5 mm pitch leaves 1.3 mm.

The pitch cannot simply be trimmed: at 2.0 mm there is no pad that is both wide
enough for a 0.45 mm annular ring and far enough for 0.5 mm clearance. **C21 is
now on a 5.08 mm land with its leads bent out** — the trick `CERAMIC-P508`
already uses — so no part changes and no rule is waived. The build sheet and the
Thai one say to bend them; `Make_RadialCan` now carries the minimum safe pitch
(2.7 mm) in its header.

**`TO220-VERT-STAG`'s stagger was not enough either.** `Make_TO220` drops pins 1
and 3 below pin 2 specifically to solve the 0.14 mm gap that 2.4 mm pads on
2.54 mm pitch would otherwise have — and at 2.0 mm of stagger Altium still found
pads 1–2 and 2–3 under the 0.5 mm rule. **Because pad 1 is square:** its corner
reaches further than a round pad's edge, so a diagonal offset buys less than it
looks like it should. True clearance was **0.361 mm**. Clearing 0.5 from the
square corner needs 2.25 mm; **the stagger is now 2.5 mm**, which measures 0.667.

Both fixes need `MakeFootprintsPT.pas` re-run **into an empty library** and the
board re-imported. Do them together.

**The check missed both, and now does not.** `check_floorplan.py` reads every
`AddPadPT` / `AddRowPT` out of the footprint source and tests pads against
Clearance, HoleSize and MinimumAnnularRing — **shape-aware**, because a
rectangle-only model reduces to `max(gap_x, gap_y)` and therefore reports a
diagonal stagger as worthless when it is not. Checked against the three known-bad
states it reproduces each one: old TO-220 0.361 mm, old radial −0.20 mm, and both
new versions clear.

**The rest of the 305 are rules that do not apply to this board**, and the answer
is to switch them off in `Tools › Design Rule Check` rather than to read past
them: **SilkToSilkClearance (129)** and **MinSolderMaskSliver (2)** — there is no
silkscreen and no soldermask here, by decision. **DisconnectedSubnets (157)** is
just the unrouted board. **MaxMinPadRndHoleSize (2)** is F1's 1.5 mm holes, which
`MakeFootprintsPT.pas` already documents as reamed out from 1.3 mm. GATE 1 asks
for a clean DRC, and it can only mean anything once the rules that do not apply
are off rather than tolerated.

**And the scale bar was being measured wrong in the process document.** Its
comment said *100.0 mm between the OUTSIDE of the two end ticks*, but the ticks
are drawn **at** 0 and 100 mm, so outside-to-outside is 100.0 plus one line
width. At 0.5 mm tracks that reads **100.5** on a perfectly printed proof — which
fails GATE 2's ±0.3 mm on its own, and the natural response to failing it is to
rescale artwork that was already correct, which is the precise disaster the bar
exists to prevent. **Centre to centre is 100.0 whatever the line width is.**
Corrected in `SetupBoardPT.pas`, `pcb-home-etch.md` and `pcb-home-etch.th.md` —
the Thai one being the copy that is actually printed and taken to the bench.

> ⚠ **The board and the sheets are not in git and cannot be.** `.PcbDoc`,
> `.SchDoc`, `.PcbLib` and `.SchLib` are all ignored, so what is committed is the
> *account* of this work, never the work. Everything since 2026-09-08 — the whole
> pre-regulator, the R23 fix, the rules, the import — exists in exactly one place
> on one disk. **Copy `hardware/STM32WL_PT/` somewhere else before closing.**

---

### Bug 1 — R23 pulls `RESET` **down**, and it is on both boards

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
☑ The same fix in `STM32WL_FE`, 2026-09-09 — a `VSENS` port of the same Bar
style, one grid step in from where the PT board put its own. **Both boards are
now right**, so nothing copied from either one inherits the pull-down again.

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

### Bug 2 — Q1's symbol was swapped, and its pads no longer match the part

**Found 2026-09-10**, reading `FrontEnd-signals.SchDoc` and the 12:26 ECO log back
after the afternoon's edits. **Not yet fixed.**

`Q1`'s schematic symbol is no longer the vendor one. It is now **`MOSFET-P` from
`Miscellaneous Devices.IntLib`**, and the two symbols number their pins
differently — read out of the `.SchDoc` pin records, not from memory:

| Symbol | Gate | Source | Drain |
|---|---|---|---|
| **`AO3401A.SchLib`** — AOS PO-00001, and what `SOT23-3-M` was drawn from | **1** | **2** | **3** |
| **`MOSFET-P`**, Miscellaneous Devices | **2** | **3** | **1** |

The schematic still *reads* right — source on `V3V3_MCU`, drain on `VSENS`, gate on
the `R1`/`R2` junction, exactly the diagram under *The rail gate* in `hardware-interface-s88.md`.
What changed is which **pad** each of those lands on, and the two ECO logs say it
plainly:

| | 2026-09-09, `AO3401A` | 2026-09-10 12:26, `MOSFET-P` |
|---|---|---|
| gate node, `R1-1` + `R2-2` | `NetQ1_1` on **Q1-1** | `NetQ1_2` on **Q1-2** |
| `V3V3_MCU` | **Q1-2** | **Q1-3** |
| `VSENS` | **Q1-3** | **Q1-1** |

`SOT23-3-M`'s pads carry the AOS drawing's numbers — 1 gate, 2 source, 3 drain —
and that footprint has not been touched since 2026-09-08. So on copper the three
nets are now **rotated one place around the part**: pad 1, the **gate**, carries
`VSENS`; pad 2, the **source**, carries the gate-drive node; pad 3, the **drain**,
carries `V3V3_MCU`.

**What the board would do.** `VSENS` has no path to a supply at all, so the rail
never comes up and everything behind it — six probes, three SHT45s, the mux — is
dead. The body diode (drain→source on a P-FET) instead feeds 3V3 into the
`R1`/`R2` node and parks it near 2.6 V, so pulling `SENS_GATE` low sinks
(3.3 − 0.7) / `R2` back into PA8 — **26 mA at `R2` = 100 Ω**, against the STM32's
20 mA per-pin limit. And the gate now sits on `VSENS`, which is to say on its own
output. That is not a switch.

**Why every audit in this document passes it.** §1 asks whether each component
carries the right footprint *name*: Q1 still says `SOT23-3-M`, and that is still
the right land for the right part. §4 compiles clean, because a symbol numbered
1–2–3 wired to a footprint numbered 1–2–3 has no error to report. §6 counted 69 of
69, every one with a `-PT` or module land, Q1 among them. **The netlist is
consistent; it is just wrong** — which is the sentence `MakeFootprintsPT.pas`
already carries in its `Make_ModTCA9548A` comment, for exactly this failure on the
mux. Same class of bug, second occurrence, and the reason that comment is in the
script.

#### What the fix actually has to touch — read before doing it

Three things read out of `FrontEnd-signals.SchDoc` on 2026-09-10 narrow the job
and change how it should be done:

**1. The footprint is already right, and already current.** Q1 carries three
models: `E3` (`PCBLIB`), `PMOS` (`SIM`), and `SOT23-3-M` (`PCBLIB`, `ISCURRENT=T`).
So the land was set back correctly after the symbol was swapped, the PCB has the
right pads, and **only the pin numbering is wrong**. The fix is narrower than
re-doing the part.

**2. Delete the `E3` model while you are in there.** It came in with `MOSFET-P`
and it is a **TO-92** land — `TO, Flat Index; 3 In-Line, Axial Leads; Body Dia.
4.6mm`. It is not current, so it does nothing today; it is one click in the
Models list from becoming Q1's footprint, on a board where Q1 is the only SOT-23
and the only part that needs magnification to solder.

**3. Swapping the symbol will break all three wires, because the two symbols are
not the same shape.** Pin positions relative to the component origin:

| | Gate | Source | Drain |
|---|---|---|---|
| `AO3401A` — a plain box, all three pins on the **right edge** | (+30, −10) | (+30, −20) | (+30, 0) |
| `MOSFET-P` — the classic glyph, **gate on the left** | (0, −10) | (+20, −20) | (+20, 0) |

Only the drain lands in a comparable place. This is very likely *why* the swap
happened — the vendor symbol is a featureless box and the stock one reads like a
MOSFET — so it is worth deciding on purpose rather than reverting on reflex.

#### Two ways to fix it, and which one this board should take

**Restore `AO3401A`. This is the one to do.** `..\Lib\AO3401A\AO3401A.SchLib`
is still attached to the project (`STM32WL_PT.PrjPcb`, `[Document11]`), so nothing
was ever missing. Its pin numbers are the AOS drawing's, which is what
`SOT23-3-M` was built from, and it is what `STM32WL_FE` carries — so both boards
match again and neither can seed this bug into a third. Cost: a box symbol, and
three wires to redraw.

**If the glyph is worth keeping**, the safe version is *not* to renumber the
placed part — `Tools › Update From Libraries` reverts that silently and without a
warning, which makes it a worse bug than the one it fixes. It is to copy the
symbol into `STM32WL_PT.SchLib` (which already exists and already holds
`MOD-DFR0570`), renumber its pins to G=1 S=2 D=3 **there**, and — the part that
matters — **give it a different name**, e.g. `AO3401A-GLYPH`. A project-local
symbol still called `MOSFET-P` that disagrees with the stock `MOSFET-P` about pin
numbers is the next person's version of this same bug.

☑ Replace Q1's symbol with `AO3401A` — **done 2026-09-10 14:48**.
☑ Redraw the three wires — gate to the `R1`/`R2` junction, source to `V3V3_MCU`,
drain to `VSENS`.
☑ The `E3` model is gone — the whole model set was replaced with the vendor
symbol's own, which took `E3` with it. See the new problem below.
☑ **The pin numbering is fixed and the 14:49 ECO proves it**: `NetQ1_1` on
**Q1-1** with `R1-1` and `R2-2`, `V3V3_MCU` on **Q1-2**, `VSENS` on **Q1-3**.
`R1-2` and `R2-1` never moved. This is the line that was asked for and it reads
correctly.

#### But restoring the symbol dragged the vendor's footprint in with it

The same ECO also says:

```
Change Component Footprint: Designator=Q1
    Old Footprint=SOT23-3-M  New Footprint=SOT-23-3L_AOS
```

`AO3401A.SchLib` carries its own PCB model, so replacing the symbol **replaced
the land as well** — it did not merely add one. `SOT-23-3L_AOS` lives in
`..\Lib\AO3401A\AO3401A.PcbLib` alongside `-L` and `-M` variants: the Ultra
Librarian density set, Least / Nominal / Most, of which the unsuffixed one is the
**nominal** IPC land.

`SOT23-3-M` is not that. `MakeFootprintsPT.pas` describes it as *SOT-23
hand-solder land (AOS PO-00001 rev N **+ 0.25 mm**)* — pads deliberately grown,
because §5 of the spec delta already calls Q1 **the one real exception** on this
board: 0.95 mm pitch needing ~0.6 mm pads with ~0.35 mm gaps, below the 0.5 mm
design rule and at the edge of what toner transfer resolves. A nominal land is
smaller than a hand-solder land by construction, on the one footprint that had no
margin to give.

**This is the second time a library binding has silently swapped a land on this
board.** §6's `MODELDATAFILE0` trap did it to C11, C19 and F1 by pointing at a
library that no longer exists; this one did it by pointing at a library that
does. Both pass a check that asks whether the footprint *name* is right, because
in both cases the name is the thing that changed.

☑ Q1's footprint set back to **`SOT23-3-M`** — the 14:55 ECO says
`Old Footprint=SOT-23-3L_AOS  New Footprint=SOT23-3-M`, and the sheet now carries
exactly one PCB model, current, **with no `MODELDATAFILE0`** — so it resolves as
`Any`, which is §6's rule and the thing that stops this recurring.
☑ **Saved.** `STM32WL_PT.PcbDoc` is the 14:56 file, after the 14:55 ECO.

#### Closed — verified out of the saved board, 2026-09-10

Read back from `STM32WL_PT.PcbDoc` itself rather than from the ECO that claimed
it, because the 14:49 round is precisely the case where those two disagreed:

| | |
|---|---|
| Q1's land | **`SOT23-3-M`** — the hand-solder land, back |
| Q1's gate net | **`NetQ1_1`** — `NetQ1_2` is gone from the file |
| Components | **69**, and the footprint tally is unchanged across all 19 lands |
| Rooms | **none** in the saved board, whatever the ECO logged |
| Design rules | all six still set; `MinimumAnnularRing` still 17.7165 mil = 0.45 mm |

**`STM32WL_FE` never diverged** — it kept the `AO3401A` symbol throughout, so both
boards agree again and neither can seed this into a third.

**The lesson worth keeping is not "check the footprint name".** Both times a land
changed under this board, the name was the thing that changed and a name check
would have caught it. What neither would have caught is the *first* version of
this bug, where the name stayed right and the pin numbers moved. The question to
ask a component is where its model is told to look, and whether its symbol's pin
numbers are the ones its datasheet uses.

> **Renumbering `SOT23-3-M` would fix this board and break the part.** The land is
> named for the AOS drawing and `MakeFootprintsPT.pas` documents it that way; a
> footprint whose pad numbers no longer match its datasheet is the next person's
> version of this same bug.

**`STM32WL_FE` is not affected.** Its `FrontEnd-signals.SchDoc` still carries the
`AO3401A` symbol — unlike Bug 1, this one was introduced here and is not inherited.

---

### Bug 3 — Q2 and Q3 are wired through their gates

**Found 2026-09-11, starting to route the 24 V corner. ☑ FIXED and verified the
same day — see *Closed* at the end of this section.**

Same root cause as Bug 2, on two more parts. Both TO-220s carry a **generic
Miscellaneous Devices symbol** — `MOSFET-P` for Q2, `MOSFET-N` for Q3 — and both
number their pins **D=1, G=2, S=3**. `TO220-VERT-STAG` numbers its pads the way
the part does, and `MakeFootprintsPT.pas` says so in the footprint's own
description: **`TAB = PIN 2 = DRAIN, LIVE`**. IRF9540N and IRF740 are both
**1 = Gate, 2 = Drain (tab), 3 = Source**.

So gate and drain are exchanged on copper. From the 2026-09-09 ECO, which is what
the board was built from:

| net | symbol pin | lands on pad | which is really… |
|---|---|---|---|
| `24V_RAW` | Q2 D = 1 | pad 1 | **the gate** |
| `NetD10_1` (R38 + D10) | Q2 G = 2 | pad 2 | **the drain, and the tab** |
| `24V_PROT` | Q2 S = 3 | pad 3 | the source — the only one right |

**What the board would do.** `24V_RAW` drives Q2's **gate** at the full bank, 18–32 V,
against a ±20 V V_GS maximum — the precise failure `R38` and `D10` exist to
prevent, arriving through the pin those two are supposed to protect. There is no
channel path from input to output at all: the only route to `24V_PROT` is the body
diode from the gate-clamp node through a 470 kΩ resistor, so **the board never
powers up**, and `Q2` dies the first time 24 V is connected. Q3 is the same shape:
its gate sits on `24V_PROT`, which `D9` clamps at up to **53.3 V**, against the
IRF740's ±20 V.

**And both tabs move.** §6 warns that *the tab of a TO-220 is the drain — Q2's tab
sits at `24V_PROT` and Q3's tab sits at `24V_PRE`, neither may touch the
enclosure*, and that **Q3's heatsink is live at `24V_PRE`**. With gate and drain
exchanged, both tabs sit on their **gate** nodes instead. Every isolation
statement written about them is wrong until this is fixed.

**Why the netlist audit passed it.** §2's check reads *Q2 drain on `24V_RAW` and
source on `24V_PROT`* — and that is true of the **symbol**. `D` and `S` are pin
*names*; the audit never compared them to the pad *numbers* the footprint uses.
Exactly Bug 2's blind spot, one sheet over.

**The sweep that found it, run over all three sheets**, comparing each symbol's
pin names against its footprint's documented pad meaning:

| part | symbol | footprint convention | |
|---|---|---|---|
| **Q2** | `MOSFET-P` D=1 G=2 S=3 | TO-220 1=G 2=D(tab) 3=S | **mismatch** |
| **Q3** | `MOSFET-N` D=1 G=2 S=3 | TO-220 1=G 2=D(tab) 3=S | **mismatch** |
| Q1 | `AO3401A` G=1 S=2 D=3 | SOT-23 per AOS drawing | ok — fixed as Bug 2 |
| Q4 | `NPN` C=1 B=2 E=3 | `BC547: 1=C 2=B 3=E` | ok |
| D9 | `Diode` A=1 K=2 | `Pad 2 = CATHODE` | ok |
| D10–D12 | `D Zener` A=1 K=2 | `Pad 2 = CATHODE (band)` | ok |
| U7 | `MOD-DFR0570` | pads named to match | ok |
| U3 | `TCA9548APWR` | pads named by chip pin | ok |

**The fix, and do not take the shortcut.** Renumbering `TO220-VERT-STAG` would fix
this board and break the footprint for every TO-220 that follows — the same
argument as Bug 2. Give Q2 and Q3 symbols that carry **their own** pin numbers,
in `STM32WL_PT.SchLib`, which already holds `MOD-DFR0570`:

☐ Draw **`IRF9540N`** (P-channel) and **`IRF740`** (N-channel) with **G=1, D=2,
S=3**, named after the parts so neither can be mistaken for the stock
`MOSFET-P`/`MOSFET-N`. Step by step:

> **1.** Open **`STM32WL_PT.SchLib`** — the project's own library, the one that
> already holds `MOD-DFR0570`. Not `..\Lib\`, which is shared with `STM32WL_FE`.
>
> **2.** `Tools › New Component`, name it **`IRF9540N`**. Do this in the **SCH
> Library** panel so you can see the component list.
>
> **3. Draw the body.** `Place › Rectangle`, a few grid squares. A plain box with
> the pin names showing is enough and is what the vendor `AO3401A` symbol is — the
> picture is not what makes a symbol correct, the numbering is.
>
> **4. Place three pins** (`Place › Pin`). Press **Tab while the pin is on the
> cursor** to set its properties before dropping it:
>
> | Designator | Name | Electrical Type | Where |
> |---|---|---|---|
> | **1** | `G` | Passive | **left** edge |
> | **2** | `D` | Passive | **right** edge, upper |
> | **3** | `S` | Passive | **right** edge, lower |
>
> The **hot end** — the end with no line into the body — must point **outward**.
> Press **Space** while placing to rotate.
>
> **Lay them out in that arrangement on purpose.** It is the same geometry as the
> stock `MOSFET-P`: gate on the left, drain upper-right, source lower-right. Q2's
> wires on the sheet already run to those three places, so keeping the positions
> means the wires stay connected when you swap the symbol — unlike Bug 2, where
> restoring a box-shaped vendor symbol broke all three and they had to be redrawn.
>
> **5. Set the component's own properties** (`Tools › Component Properties`):
> **Default Designator** `Q?`, **Comment** `IRF9540N`, and a Description worth
> reading later, e.g. *P-channel TO-220, 1=G 2=D(tab) 3=S per datasheet*.
>
> **6. Attach the footprint.** In the same dialog, `Add › Footprint` → **Browse**
> → `STM32WL_Pt.PcbLib` → **`TO220-VERT-STAG`** → and in the **PCB Library** group
> choose **`Any`**. That last field is §6's rule and it is the one that stops a
> land silently swapping itself later.
>
> **7. Repeat for `IRF740`** — identical pin numbering, N-channel, Comment
> `IRF740`. `Tools › Copy Component` onto the first one and edit the copy is
> quicker than drawing it twice.
>
> **8. Save the library**, then **`Tools › Update Schematics`** from inside it, or
> repoint each part by hand: on `Buck-regulator.SchDoc`, double-click Q2 →
> **Design Item ID** → `...` → pick `IRF9540N` from `STM32WL_PT.SchLib`. Same for
> Q3 → `IRF740`.
>
> **9.** Check the three wires at each part are still attached — a wire that only
> *looks* connected is the failure here. `Project › Compile` must give **zero
> errors and zero warnings**; a floating pin shows up there.
>
> **10. THE CHECK THAT MATTERS: the re-import ECO must contain net changes.**
> It must say, for Q2, `24V_RAW` moving to **Q2-2** and `NetD10_1` to **Q2-1**;
> for Q3, `24V_PROT` to **Q3-2** and `NetC22_2` to **Q3-1**.
>
> **An ECO with no `Added Pin To Net` lines at all means the fix did not take** —
> and it will look like a success, because the symbol names and descriptions all
> change and the dialog reports work done. That happened on the first attempt
> here, 2026-09-11 12:53.

#### The trap in step 4, which caught this fix on its first pass

The stock `MOSFET-P` and `MOSFET-N` place their pins **1 = upper right, 2 = left,
3 = lower right**, and the wires on the sheet run to those three *places*. Drawing
the replacement by putting **pin 1 where pin 1 used to be** reproduces the old
mapping exactly: the left wire still carries the gate node and still lands on pin
2, the upper-right wire still carries the drain net and still lands on pin 1. The
symbol now *says* G=1 D=2 S=3 while the netlist is byte-for-byte what it was, so
Altium finds nothing to push and the ECO comes back with no net changes.

**Place the pins by NAME, not by number.** The wire already at the left is the
gate wire, so **`G` goes on the left** whatever number it carries. The wire at the
upper right is the drain wire, so **`D` goes upper right**. Only then does the
number under each wire actually change, which is the entire point of the fix.

| | stock symbol | what step 4 asks for |
|---|---|---|
| left | G, numbered 2 | **G, numbered 1** |
| upper right | D, numbered 1 | **D, numbered 2** |
| lower right | S, numbered 3 | S, numbered 3 |

**If the symbols are already drawn the wrong way round**, the repair is to swap
where pin 1 and pin 2 sit inside `IRF9540N` and `IRF740` — two drags in the
library, `Tools › Update Schematics`, re-import — rather than to move wires on
the sheet.
☐ Point Q2 and Q3 at them, keeping `TO220-VERT-STAG` as the footprint with the
**PCB Library** group set to **`Any`**.
☐ Re-import, and check the ECO: `24V_RAW` on **Q2-2**, `NetD10_1` on **Q2-1**,
`24V_PROT` on **Q2-3**.
☐ **Confirm the pinout against the datasheets of the parts actually bought.**
TO-220 pin order is per-part, not per-package; 1=G 2=D 3=S is right for the
IRF series and is not a universal rule.

#### Closed — verified out of the saved board, 2026-09-11 13:06

The 13:06 ECO carries the four moves, both directions:

```
Removed Pin From Net: 24V_RAW  Q2-1     Added Pin To Net: NetD10_1 Q2-1
Removed Pin From Net: NetD10_1 Q2-2     Added Pin To Net: 24V_RAW  Q2-2
Removed Pin From Net: 24V_PROT Q3-1     Added Pin To Net: NetC22_2 Q3-1
Removed Pin From Net: NetC22_2 Q3-2     Added Pin To Net: 24V_PROT Q3-2
```

And the board agrees, read back pad by pad:

| | pad 1 = **gate** | pad 2 = **drain / tab** | pad 3 = **source** |
|---|---|---|---|
| **Q2** | `NetD10_1` — R38 + D10 | `24V_RAW` | `24V_PROT` |
| **Q3** | `NetC22_2` — R41/D11/C22/Q4 | `24V_PROT` | `NetD12_1` → R42 → `24V_PRE` |

**Q2 is now the reverse-polarity hookup and not the ideal-diode one**: drain to the
supply side, source to the load, which is what §5's *Q2 — the reverse-polarity FET
is wired backwards on purpose* requires. The gate clamp reaches the gate. Both tabs
are back on their drains, so §6's warnings about Q2's tab at `24V_PROT` and Q3's
live heatsink read true again.

**The whole-board sweep of this bug class is clean.** Every symbol's pin names now
agree with its footprint's pad convention: `IRF9540N`/`IRF740` G=1 D=2 S=3 against
TO-220 1=G 2=D(tab) 3=S; `AO3401A` G=1 S=2 D=3 against the AOS drawing; `NPN`
C=1 B=2 E=3 against `BC547: 1=C 2=B 3=E`; `Diode` and `D Zener` A=1 K=2 against
`Pad 2 = CATHODE`. **Three occurrences of one mistake, on Q1, Q2 and Q3, and no
fourth.**

---

## Routing — first full check, 2026-09-11 23:35

Read out of `STM32WL_PT.PcbDoc`: **474 copper tracks**, 260 top and 214 bottom,
and **`DisconnectedSubnets` is down from 157 to 6**.

**The surge path is right, which is the part that had to be.** Every segment in
the 24 V corner is **3.00 mm**: `24V_IN` ×2, `24V_RAW` ×4, `24V_PROT` ×8 and
**`GND` ×16**, with a single 1.5 mm GND segment left. §5's *give D9's anode and
C11's negative their own wide copper back to J14's ground pin* is satisfied.

**`NetF1_1` is now `24V_IN`** on the schematic and the board, and **the `W_POWER`
scope was extended to `InNet('24V_IN') or InNet('24V_RAW')`** alongside
`24V_PROT`/`24V_PRE`/`GND`. That closes a real gap: the surge from J14 reaches D9
*through* F1 and Q2, so those two nets carry the same 11.3 A as `24V_PROT` and the
original rule did not cover them.

**`W_POWER`'s minimum was lowered 3.0 → 1.5 mm.** Recorded as a decision, not
drift: the 3 mm figure comes from D9's surge and from keeping the clamp loop's
inductance low, and **the corner where that applies is still 3 mm throughout**.
What sits at 1.5 mm is `24V_PRE` (all 10 segments) and GND away from the corner —
`24V_PRE` is behind Q3's ~130 mA limiter and never sees a surge, so 1.5 mm there
is already two orders of margin.

### Four things still open

**1. The ten hand-made vias are the wrong size.** Free pads used as vias is the
right idea on an unplated board. These are **1.52 mm pad with a 3.20 mm hole** —
the hole is bigger than the pad, so there is **no annular ring at all** (−0.84 mm),
and 3.2 mm is not one of the four drill sizes the process commits to. They are
also `Plated = False`, which is what raises the ten `TUnplatedPad` violations;
every footprint pad on this board is deliberately `Plated = True`
(`MakeFootprintsPT.pas`: *a lie about this board, told on purpose*) because Altium
needs it for connectivity.

> Set all of them to **2.0 mm pad / 0.8 mm hole, plated** — Stage 1's via spec,
> and a 0.6 mm ring. That also clears every `TUnplatedPad` and returns
> `TMaxMinPadRndHoleSize` to F1's documented two.

**Eight of them, as of 2026-09-12 00:42** (two VSENS ones were removed at 00:42,
taking `DisconnectedSubnets` to 5):

| | net | | net |
|---|---|---|---|
| (19.81, 48.13) | `V3V3_MCU` | (69.98, 64.52) | `NetD12_1` |
| (43.69, 72.26) | `GND` | (73.03, 30.48) | `DQ_P4` |
| (49.53, 78.49) | `GND` | (77.98, 54.99) | `NetD12_1` |
| (59.56, 34.54) | `DQ_P4` | (96.90, 64.14) | `V3V3_MCU` |

**All eight have room to grow**, checked against every track and pad of a
different net: the tightest is 1.19 mm to an `I2C_SDA` track, against a 0.5 mm
rule. Nothing needs moving first.

**Step by step:**

> **1.** Press **`Q`** so the editor is in mm.
>
> **2. Select them by hole size**, which is unambiguous here: the next largest
> hole on the board is F1's 1.5 mm, so nothing else can be caught.
>
> **Use Find Similar Objects — no query syntax to get wrong.** Click one of the
> eight (there is one on GND at **(43.69, 72.26)**), right-click › **Find Similar
> Objects**, then set **Object Kind = `Same`** and **Hole Size = `Same`**, and
> leave **everything else `Any`** — in particular **Net** and **Layer**, which
> default to `Same` and would cut the selection down to one net. Scope **Current
> Document**, tick **Select Matching** and **Run Inspector**.
>
> **If you prefer the PCB Filter panel, mind the units.** `HoleSize > 2mm` is a
> syntax error in this build — the parser reads the `2`, meets `mm` and reports
> *expected )*. **Query dimensions are in mils**, whatever the editor is
> displaying, so 2 mm is written **79**:
>
> ```
> IsPad And (HoleSize > 79)
> ```
>
> If that selects **0** objects rather than 8, this build is reading the number as
> mm instead — use `IsPad And (HoleSize > 2)`. Either way the count tells you
> which happened, which is why step 3 exists.
>
> **3. Confirm the Inspector title says 8 object(s).** Do not skip this: the
> scale-bar step earlier used a width-based selector that quietly caught 20
> objects instead of 8, and nothing announced it.
>
> **4. In the PCB Inspector set all four fields**, while they are still selected:
>
> | field | to |
> |---|---|
> | X-Size | `2mm` |
> | Y-Size | `2mm` |
> | Hole Size | `0.8mm` |
> | **Plated** | **ticked** |
>
> Change the hole **last**, or at least do not deselect first — once it is 0.8 mm
> the `HoleSize > 2mm` query no longer finds them.
>
> **`Plated` is the deliberate lie.** There is no plating on this board and every
> hole is a wire soldered on both faces — but Altium uses the flag for
> connectivity, and `False` makes the pours and nets refuse to connect through
> the pad. `MakeFootprintsPT.pas` sets it `True` on every footprint pad for
> exactly this reason and says so; these eight were created outside that and
> missed it.
>
> **5.** Clear the filter — **`Shift+C`**, or the **Clear** button at the bottom
> right — then re-run the DRC.
>
> ✓ **Check:** `TUnplatedPad` **gone**, `TMaxMinPadRndHoleSize` back to **2**
> (F1's reamed pair), and **no new `Clearance`**.

#### What went wrong on the first attempt, 2026-09-12 00:59

**The eight vias came out right** — 2.0 mm pad, 0.8 mm hole, +0.60 mm ring, and
`DisconnectedSubnets` fell 5 → 3. But the selector caught **ten** objects, not
eight, and the two extra were **F1's fuse clips**.

The threshold used took in everything above about 1.4 mm, and F1's holes are
**1.5 mm** — the one deliberate exception on this board, reamed up from 1.3
because 1.5 is not in the four-bit drill set. So F1 went from **3.0 mm pad /
1.5 mm hole** to **2.0 / 0.8**, and a 0.8 mm hole will not take a fuse clip's leg.

Two signatures give it away without opening the board:

- **`TMaxMinPadRndHoleSize` vanished entirely.** It should have fallen to 2, not 0
  — those two *are* F1, and a rule going quiet is not always good news.
- **`TUnplatedPad` went 8 → 10**, not 8 → 0. The `Plated` tick never got set, and
  F1's two pads, which the footprint had set plated, were dragged unplated along
  with the rest.

**Nothing else was touched**, confirmed against the hole histogram: 1.0 mm ×92,
1.1 mm ×38, 1.3 mm ×6 all unchanged.

**The repair:**

> **1. Put F1 back with `Tools › Update From PCB Libraries`**, selecting **F1**.
> That restores its pads from `STM32WL_Pt.PcbLib` — 3.0 mm, 1.5 mm hole, plated —
> in one operation with nothing typed. It is the same tool that finally pushed
> Q2/Q3's stagger, and for the same reason: it is the only one that compares
> footprint *geometry* rather than names.
>
> **2. Tick `Plated` on the eight vias.** Select them as before — they are now the
> only 2.0 mm pads with a 0.8 mm hole that are not part of a component — and set
> **Plated** in the Inspector. Their sizes are already correct; this is the one
> field that did not take.
>
> ✓ `TUnplatedPad` **0**, `TMaxMinPadRndHoleSize` **2**.

☑ **Both done and verified, 2026-09-12 01:13.** F1 reads **3.00 mm pad / 1.50 mm
hole** again; the hole set is back to **0.8 ×87, 1.0 ×92, 1.1 ×38, 1.3 ×6** plus F1's
**1.5 ×2**; eleven pads sit at 2.0/0.8 — the eight vias and Q4's three legs — and
**`TUnplatedPad` is gone**.

☑ **Item 2 is closed too.** All three keep-out rectangles are back and **no track
crosses U3's footprint** any more; the six branch segments were re-routed. GND
also gained copper in the process: **87 segments at 3.00 mm**, up from 51.

> **The rectangles were redrawn by hand, not by the script**, and sit a little
> inside it — U7 at (101.98, 44.58)–(114.76, 59.44) and U3 at
> (105.09, 68.07)–(119.29, 97.03) against `DrawKeepoutsPT`'s (101, 43.5)–(115, 60.5)
> and (105, 66.5)–(119, 97.5). Both still clear their modules' pad columns, so
> they do their job. But **running `DrawKeepoutsPT` again would now lay a second,
> slightly different set on top** — run `ClearKeepoutsPT` first, or leave it alone.
> (There are 13 tracks rather than 12 because U7's left edge is drawn in two
> collinear pieces.)

**The DRC is down to two categories: `DisconnectedSubnets` ×3 and
`MaxMinPadRndHoleSize` ×2 — F1's documented reamed pair.** Everything else is
clear.

> **Then they have to reach the build sheet.** Eight wire links, drilled 0.8 mm
> and soldered both faces, are eight assembly steps that nothing on site currently
> describes — [`build-sheet-proto.md`](build-sheet-proto.md) and its Thai mirror
> are the only things at the bench, and a missed wire link is eight silent open
> circuits. Add them once the positions are final.

**2. U3's keep-out is gone and six tracks run under it.** Only 8 keep-out tracks
remain — Q3's four and U7's four. `NetJ9_2` ×2 and `NetJ9_3` ×2 on the top layer
and `NetJ10_2` ×2 on the bottom now cross U3's footprint, which §6 rules out.
They are U3's own branch nets leaving its pads, so the path is the natural one —
but **this board has no soldermask**, and the module's underside sits over them.
Either re-route the six outside the pad columns, or record the decision that the
header's standoff is clearance enough.

**3. There is no GND pour** — `Polygons6` is empty. GND was hand-routed instead.
**Decided 2026-09-12: pour it**, over the tracks, as `pcb-home-etch.md` rule 4
says. The routed ground stays and the pour merges with it.

#### Two things to set before pouring, both of which bite on this board

**The `PolygonConnect` relief spokes are 10 mil — 0.254 mm — and the board's
minimum feature is 0.5 mm.** That is half the narrowest thing toner transfer
resolves here, so every thermal spoke on every ground pad would come out thin,
ragged or missing. Raise the relief conductor width to **0.6 mm**, above the
0.5 mm `Width` minimum with margin for the etch, and set the air gap to
**0.5 mm** to match `Clearance`.

**And the surge-return pads must be Direct, not Relief.** §5 asks for D9's anode
and C11's negative on wide copper straight back to J14's ground pin; four 0.6 mm
spokes is not that, and it is exactly the inductance the clamp cannot afford.
Add a second `PolygonConnect` rule at **priority 1**, connect style **Direct**,
scoped

```
InNet('GND') And (InComponent('D9') Or InComponent('C11') Or InComponent('J14'))
```

and leave the relief rule as the lower-priority catch-all. Without this the one
node the spec calls out as *the place where "ground is just ground" is wrong*
reaches ground through twelve hair-thin spokes.

**Then the stitching.** Rule 4 wants **8–12 vias** tying the two pours, densest in
the 24 V corner. Only two of the eight existing hand-made vias are on GND, so
most of these are new: **2.0 mm pad, 0.8 mm hole, plated**, same as the eight.

**4. Six connections are still unrouted.** — **three** as of 2026-09-12 01:13.

---

## Where the board stands — 2026-09-10 22:10, verified

Read back out of `STM32WL_PT.PcbDoc` itself, not from the dialogs that reported
it:

| | |
|---|---|
| Components | **69**, every one at the scripted position, rotation and lock state |
| Locked | **16** — J1–J7, J9–J14, CN6, U3, U7 |
| C21 | on **`RADIAL-D5-P508`**, pads at ±2.54 mm |
| Q2 / Q3 | pad stagger **−2.5 mm**, the library geometry actually on the board |
| Keep-outs | **12 tracks at 0.25 mm**, three closed inset rectangles, no duplicates |
| Scale bar | **8 copper tracks at 0.50 mm** |
| Design rules | all six, `MinimumAnnularRing` still 0.45 mm |
| DRC | **`TDisconnectedSubnets` and `TMaxMinPadRndHoleSize` only** |

**Both survivors are expected and neither is a defect.** `DisconnectedSubnets` is
the board being unrouted — it empties as you route. `MaxMinPadRndHoleSize × 2`
is F1's two 1.5 mm holes, which `MakeFootprintsPT.pas` documents as reamed out
from 1.3 mm because 1.5 is not in the four-bit drill set. **`ShortCircuit`,
`Clearance`, `ComponentClearance`, `MinWidthStubTrack`, `SilkToSilk` and
`MinSolderMaskSliver` are all gone from the file.** That is GATE 1.

`check_floorplan.py` says **PASS**. Run it before Altium any time
`PlacePartsPT.pas` changes.

**Five defects were found and fixed between placement and this line**, and four
of the five were the same mistake wearing different clothes — something checked
by **name** while the thing behind the name had changed:

| | |
|---|---|
| Q1's symbol | pin numbers moved; footprint name unchanged |
| Q1's footprint | vendor symbol brought its own land; the name is what changed |
| C21's land | `Make_RadialCan` hard-codes a pad the pitch parameter does not police |
| TO-220 stagger | a footprint edited in place, so the ECO saw no name change and shipped nothing |
| The keep-outs | drawn on the module body, so a module's own pads violated them |

---

## Routing — the order to do it in

`pcb-home-etch.md` Stage 1 gives the five rules; this is the sequence they imply
for **this** placement. The board has **38 nets and 217 through-holes**, and the
via budget is **under 30** — so route in the order that spends copper where it is
least negotiable first.

**Everything below is on the BOTTOM layer** unless it says otherwise. That is
rule 1, and it is not a preference: every through-hole part is soldered from the
bottom.

**A. The 24 V corner, and nothing else until it is done.** `24V_RAW` → `NetF1_1`
→ `24V_PROT` → `24V_PRE`, following the physical chain J14 → F1 → Q2 → D9 → C11
→ Q3 → U7. The `W_POWER` rule already forces **3 mm** on `24V_PROT` and `24V_PRE`,
so these are the widest, least steerable tracks on the board and everything else
routes around them. Give **D9's anode and C11's negative their own wide copper
straight back to J14's ground pin**, and keep that return off the analog ground
and off R25 — §5's one place where "ground is just ground" is wrong.

**B. `V3V3_MCU`** — 6 pads: U7's two output pins, CN6-4, and Q1's source with R1.
Short and in one corner.

**C. `VSENS`, and treat it as a third power net.** **32 pads** — the second
biggest net on the board after GND. It feeds all six DQ pull-ups, U3's `VIN`, R23,
C1, C2, all four of J9–J12 pin 4 and all six of J1–J6 pin 2. It is a star that
crosses the whole board, so it wants routing early, while there is still room.

**D. The six probe channels.** Each one is `DQ_Pn` (J7 → the series resistor) then
`NetDn_1` (series resistor → pull-up → TVS → the connector). They run as six
parallel drops from J7's left-hand field down to J1–J6 along the bottom, and they
never cross each other if they are done in order. **J7's 2×20 field is a wall**
(rule 3) — route around it, not through it.

**E. I2C.** `I2C_SDA` and `I2C_SCL` from J7 to U3, picking up R15/R16 on the way;
then the four branch pairs `NetJ9_2/3` … `NetJ12_2/3` from U3 out to J9–J12 on the
right edge, each pair picking up its own 2.2 k.

**F. The leftovers**, in any order: `SENS_GATE`, `VBAT_SENSE`, `DBG_TX`, `DBG_RX`,
`NetQ1_1`, `NetR23_2`, `NetD10_1`, `NetD12_1`, `NetC22_2`.

> **A keep-out must never enclose the pads of the part it protects.** Q3's
> heatsink rectangle did — 16 × 16 mm centred on the part, swallowing all three
> of its pads — and a pad inside a keep-out cannot be routed to at all. It was
> found by trying to route `24V_PROT` from C11 to Q3 and being refused.
>
> Fixed 2026-09-11 to **(62, 36) → (78, 46)**, below the pads rather than around
> them. **Q3 is placed at 180°**, and `TO220-VERT-STAG` puts its body 1.60–6.30 mm
> *above* the pad row, so at 180° the body and the heatsink clipped to it sit
> *below*: y 41.6–46.4 against pads at y 46.8–51.7. The rectangle stops at 46.0
> and clears pad 2 by 0.68 mm. **Rotate Q3 back to 0° and this rectangle becomes
> wrong** — keep-outs are drawn at absolute coordinates and do not follow parts.
>
> `check_floorplan.py` now tests **containment**, not only distance to the edges.
> Measuring edges alone says nothing about a trapped pad: the further inside it
> sits, the healthier it looks. Against the old rectangle the check reports all
> three Q3 pads; against the new one, none.

**G. Pour GND on both layers, then stitch.** 8–12 stitching vias spread across the
board, most densely in the 24 V corner where the surge return matters.

> **You never route a GND track on this board, and the ratsnest will not tell you
> that.** GND is 47 pads — the biggest net here — and Altium draws a connection
> line for every one of them, which reads like 47 tracks waiting to be drawn. They
> are not. Rule 4 is that GND arrives as **copper poured on both faces**; the pour
> satisfies those connections and the ratsnest lines disappear when it is placed.
>
> This is the point at which someone tries to draw a track from C11's negative to
> U3 and finds a keep-out rectangle in the way. **Those two share GND and nothing
> else** — C11 is on `24V_PROT`, U3 runs on `VSENS` — so there was never a track
> to draw. The rectangle is doing its job.
>
> **The pour reaches every GND pad**, checked against the placed board: all 47 sit
> **outside** the three keep-outs, none within half a pad of an edge. The module
> rectangles are inset between the pad columns precisely so the modules' own eight
> GND pads stay reachable from outside.

**How to place the pours.** `Place › Polygon Pour`, one per copper layer, net
**GND**, **Pour Over All Same Net Objects**, **Remove Dead Copper** on. Draw each
to the board outline. Then the stitching vias, then repour (`Tools › Polygon
Pours › Repour All`) and re-run the DRC — `DisconnectedSubnets` should fall by
roughly the GND count in one step.

### The via budget, counted off the placed board

**135 of the 217 through-holes — 62 % — sit under a component body**, so they can
only be soldered from the bottom. Rule 2 says no top-layer track may terminate on
one of those, so each needs a via beside it *if* its net has to reach the top.

- **36 are GND**, and rule 4 is what makes them free: with copper poured on both
  faces they reach ground on the bottom directly. That is the whole argument for
  pouring both layers rather than one — 36 vias would be most of the budget.
- **65 are on signal nets.** These are the ones to watch, and they cluster:
  **U3's 24 pads are all covered**, as are **J7's 40** and both modules'. If a
  branch pair has to hop to the top to get past something, the hop happens at a
  via *beside* the pad, never at the pad.
- **34 carry no net at all** — J7's unused positions and U3's unused channels.
  They are drill work and nothing else.

**C21 came off this list** when its leads were bent out to 5.08 mm to fix the short
in its land: its pads are now clear of the can, so they can be soldered from the
top like any axial part.

**Count vias as you go and stop at 30.** Two per signal crossing, and the board is
160 × 120 mm for 38 nets — the answer to a crossing is almost always to route
around it.

---

## The footprint rebuild — do these in order

Two footprints changed (`RADIAL-D5-P20` → `RADIAL-D5-P508`, and
`TO220-VERT-STAG`'s stagger), which means the library is rebuilt and the board
re-imported. Everything below has a check at the end of it; do not carry on past
one that fails.

**0. Back up `hardware/STM32WL_PT/` first.** It is not in git and cannot be. This
is the step that is skipped because it is not interesting.

**1. Empty `STM32WL_Pt.PcbLib`.** Open it, select all 18 footprints in the **PCB
Library** panel, delete, save.

> **Why empty it rather than edit the two by hand.** `MakeFootprintsPT.pas` says
> in its own completion message: *re-run only into an empty library, or you get
> duplicates*. And hand-editing two footprints is four pad moves and a rename
> that nothing checks — which is the shape of every bug in this document. The
> script is the source of truth; let it be the source.

**2. Rebuild.** Click the `STM32WL_Pt.PcbLib` tab so it is the **active
document**, then `DXP › Run Script...` → **`CheckEnvironmentPT`** (it names the
library in front — read it) → then **`MakeFootprintsPT`**. Save.

> ✓ **Check:** the panel lists **18** footprints, **`RADIAL-D5-P20` is gone**, and
> **`RADIAL-D5-P508`** is there. The script's message says `RADIAL-D5-P508 C21
> LEADS BENT`.

**3. Point C21 at the new land — the step that is easy to miss.** C21 is on
**`Buck-regulator.SchDoc`**. Its model still names `RADIAL-D5-P20`, which no
longer exists, so it will import as *Footprint Not Found*. Double-click C21 →
**Models** → change the footprint to **`RADIAL-D5-P508`** → **Edit...** → set the
**PCB Library** group to **`Any`**. Save.

> `Any` is §6's rule and it is not decoration: a model bound to a named library
> is how C11, C19 and F1 broke, and how Bug 2's land silently swapped itself.

**4. Compile.** `Project › Compile PCB Project STM32WL_PT.PrjPcb`.

> ✓ **Check:** zero errors, zero warnings, as in §4.

**5. Re-import.** `Design › Update PCB Document STM32WL_PT.PcbDoc` → **Validate
Changes** → **untick `Add Rooms`** → **Execute Changes**.

> ✓ **Check:** the ECO changes **C21**'s footprint and touches **no pins and no
> nets**. Any `Added Pin To Net` means something moved that should not have.

**5b. `Tools › Update From PCB Libraries...` — and this is not optional.**

> **The ECO in step 5 will NOT update Q2 and Q3, and it will not tell you so.**
> `Design › Update PCB Document` compares the schematic to the board: the
> netlist, and footprint **names**. It does not compare the board's footprint
> **geometry** against the library. C21 came across because its name changed
> — `RADIAL-D5-P20` → `RADIAL-D5-P508`. **`TO220-VERT-STAG` kept its name and only
> its pads moved, so nothing looked different and the old 2.0 mm stagger stayed
> on the board.** Verified by reading the pads back: C21 landed on ±2.54 mm, Q2
> and Q3 were still at −2.00.
>
> Same shape as every other bug in this document — a check that asks after a
> **name** passing while the thing behind the name has changed.

On the PCB document run **`Tools › Update From PCB Libraries...`**, select **Q2**
and **Q3** (or all components — it only lists what actually differs), and execute.

> ✓ **Check:** the dialog lists Q2 and Q3 and nothing surprising. Afterwards
> their outer pads sit **2.5 mm** below the centre pad, not 2.0.

**6. Re-place.** If `PlacePartsPT.pas` is open, **close and reopen its tab** —
Altium runs the compiled copy it already has. Then `CheckPlacementPT` (expect 69
components, origin 20/20), then **`PlaceFloorplanPT`**.

> **Do NOT run `DrawKeepoutsPT` again.** The twelve tracks are already on the
> board and it has no duplicate check; two coincident keep-outs look exactly
> like one.

**7. Widen the scale bar to 0.5 mm.** 0.4 mm exists nowhere else on the board, so
selecting by width is safe — but **check the count says 8**: a selector matching
on width alone also catches the twelve keep-out tracks at 0.25 mm if it is loosened,
and that is what happened on the first pass. Step 7b puts them back.

**7b. Redraw the keep-outs.** Reload the script tab, run **`ClearKeepoutsPT`**
(expect *Removed 12*), then **`DrawKeepoutsPT`** once. This does two things: it
restores the 0.25 mm width, and it lays down the **inset** rectangles — see below.

**7c — what was wrong with the first rectangles.** They were drawn on each
module's **body outline**, which passes **1.53 mm from U3's own outer pad
centres**. A 0.95 mm pad radius and a 0.125 mm half-track leave **0.46 mm**
against a 0.5 mm Clearance rule — so **the module's own pads violated the keep-out
drawn to protect it**, from the moment it was drawn. Widening the tracks to 0.5 mm
in step 7 made it 0.33 mm and more visible, but it was never the cause.

What §6 actually asks is that no track crosses **under** the module, and a track
cannot get in among the pad columns anyway. So the rectangles now span **between
the pad columns**, inset to clear the pads by ~0.94 mm:

| | was (body outline) | now (between the pad columns) |
|---|---|---|
| U3 | 101.0, 66.5 → 123.0, 97.5 | **105.0, 66.5 → 119.0, 97.5** |
| U7 | 96.75, 43.5 → 119.25, 60.5 | **101.0, 43.5 → 115.0, 60.5** |
| Q3 heatsink | 62.0, 40.0 → 78.0, 56.0 | unchanged |

`check_floorplan.py` now measures every pad against every keep-out edge, which is
the check that was missing. Against the old rectangles it reports the four U3 pads
at 0.46 mm and 0.33 mm respectively; against the new ones, nothing.

 Click one of the eight artwork tracks, then
`Edit › Find Similar Objects` → match on **layer** and **width 0.4 mm** → select
all → set **Width = 0.5 mm** in the Properties panel. They are the 100 mm bar and
its three ticks, on Top and Bottom.

**8. Switch off the two rules that do not apply.** `Tools › Design Rule Check`,
and in the **Rules To Check** list untick **`SilkToSilkClearance`** and
**`MinSolderMaskSliver`**. This board has neither silkscreen nor soldermask, by
decision — those 131 violations are not findings, and leaving them on is how the
one real finding gets lost.

**9. Run the DRC and read the report, not the Messages panel.** The report opens
in a browser; the Messages panel can look empty while the report is not.

> ✓ **Check:** the only categories left are **DisconnectedSubnets** (the board is
> unrouted — these disappear as you route) and **MaxMinPadRndHoleSize × 2** (F1's
> 1.5 mm holes, reamed from 1.3 mm, documented in `MakeFootprintsPT.pas`).
> **Anything else is new.** In particular there must be **no ShortCircuit** and
> **no Clearance** rows.

**10. `File › Save All`, and back up `hardware/STM32WL_PT/` again.**

**11. Routing** — [`pcb-home-etch.md`](pcb-home-etch.md) Stage 1.

**Any time you move something in `PlacePartsPT.pas`**, run
`python hardware/scripts/check_floorplan.py` before Altium. It currently says
PASS, and it checks the things a DRC can only tell you after the fact.

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
☑ `Tools » Footprint Manager` shows **no component with a missing model** — and
§6's import is the stronger form of the same check, since it resolves each land
rather than only naming it. It found three this did not; see the note there.

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

☑ Suppressed at the pin, 2026-09-09 — a `Place » Directives » Generic No ERC` on
`U3` pin 3, on **both** boards, rather than by lowering *Nets with no driving
source* in `Project » Project Options » Error Reporting`, which is global and
would hide a genuinely undriven input somewhere else. `FrontEnd-signals.SchDoc`
already carried 26 No-ERC directives; this is `I2C-sensors.SchDoc`'s first.
☐ Check the compile output for **floating power ports**. This project carries all
connectivity on power ports, so a mistyped `24V_PRE` does not error — it silently
creates a second, unconnected net with a similar name. Look at the Net list and
confirm there is exactly one `24V_PRE`.
☑ **Remove the three libraries the proto no longer uses** — `DMP6023LE-13`,
`SMBJ33A`, `BZX84C12`. They were kept attached deliberately until now, because the
copied sheets still carried those symbols. They come off *after* the parts are
swapped, not before (`pcb-altium.md` §2.1a). **2026-09-09: two of the three are
gone** (`DMP6023LE-13` and `SMBJ33A`, both halves each), and `TESTPOINT.SchLib`
went with them, settling §3's second question in the spec's favour. **`BZX84C12`
is still attached** — it is two files under one folder name,
`diode-nc_pin.SchLib` and `SOT-23.PcbLib`, which is what made it the easy one to
miss. **Gone too, later the same day.** What is left attached is exactly what is
still used: `CDSOD323-T05LC` for D1–D6, `AO3401A` for Q1, `TCA9548APWR` for U3's
symbol, plus the project's own `STM32WL_PT.SchLib` and `STM32WL_Pt.PcbLib`.
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

☑ `Design » Import Changes From STM32WL_PT.PrjPcb` — 2026-09-09, **69 of 69**.
☑ Every component arrived with a `-PT` footprint or one of the two module
footprints, and nothing arrived with an FE-board land. Counted off the `.PcbDoc`:
30 axial resistors, 5 `CERAMIC-P508`, 2 `RADIAL-D8-P35`, 1 `RADIAL-D5-P20`, 3
`DO41-P762`, 1 `DO15-P1270`, 2 `TO220-VERT-STAG`, 1 `TO92-INLINE-P254`, 6+4+1
Phoenix, both module lands, and the seven parts left deliberately SMD.

> **The three that fail here, and why a footprint audit misses them.**
> **C11, C19 and F1** report *Footprint Not Found* on the first import
> (`RADIAL-D8-P35` twice, `FUSEHOLDER-5X20-P226` once). The footprints are present
> in `STM32WL_Pt.PcbLib`; what is wrong is that those three models carry an explicit
> `MODELDATAFILE0` binding them to `C:\Users\Public\Documents\Altium\Projects\WaterTempNode_FE\WaterTempNode_FE.PcbLib` — a library from before the projects moved
> into the repo, and no longer on disk. Every other model on the board carries no
> library binding at all and is searched as *Any*, which is why the other 67
> components import clean.
>
> **They are exactly the three whose footprint name did not change.** C11, C19 and
> F1 were already through-hole on the FE board, so §1's table asks them for a name
> they already had — nothing was edited, and not editing them is what preserved the
> old binding. A check that every component's *current* model is the right **name**
> passes on all three, because the name was never the problem. The question a
> footprint audit has to ask as well is **where that name is told to look**.
>
> Fix each in `Buck-regulator.SchDoc`: double-click the part, `Models` →
> `RADIAL-D8-P35` (or `FUSEHOLDER-5X20-P226`) → **Edit...** → in the **PCB Library**
> group choose **`Any`**. The preview fills in as soon as it resolves.
>
> **Do not fix it by attaching `WaterTempNode_FE.PcbLib`**, even if a copy turns up.
> §2.1a is why the PT project carries no FE-era reference at all, and that library
> predates `MakeFootprintsPT.pas` — its lands have fab-house pads, which is the one
> thing a hand-drilled hole cannot use.
☐ Then [`pcb-home-etch.md`](pcb-home-etch.md) Stage 1 for the layout rules —
bottom layer is the real board, GND poured on both faces, nothing routed between
2.54 mm pins, and vias counted as you go.

---

## Not yet written

- `pcb-altium-pt.th.md` — the Thai mirror. The two documents that are read at a
  bench rather than at a desk (`pcb-home-etch` and `build-sheet-proto`) already
  have theirs; this one is a desk document, which is why it is last.
