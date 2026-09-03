# Drawing this board in Altium Designer 17

A step-by-step route from the schematic sheets that already exist to a set of files
JLCPCB will accept, written for someone who has never opened Altium.

**This document does not decide anything.** Every value, part number, package and
placement rule in it comes from [`hardware-interface.md`](hardware-interface.md) —
§4a is the parts list, §3 is the front-end circuit, §5 is the power section, §6 is
the cabling. Where this file and that one disagree, that one is right and this one
is a bug. What this file adds is the *order of operations* and the twenty or so
places where Altium will silently do the wrong thing.

**Scope:** the front-end PCB only. The gateway is a bare NUCLEO-WL55JC1 with a USB
lead and has no board of its own.

**Assembly:** hand-soldered, five boards. That decision is upstream of a lot of
what follows — it is why the passives are 0805 and not 0603, why there are no
fiducials, and why no pick-and-place file is generated.

---

## 0. Where you actually are

This is not a blank start. Read from the project files themselves:

| Sheet | Last saved | What is on it |
|---|---|---|
| `FrontEnd.SchDoc` | 31 Aug, 01:30 | 58 refs — **the pre-revision-2.0 board**: U1a/U1b/U1c, U2, TVS1, C12–C14 |
| `Connector.SchDoc` | 3 Sep, 15:31 | J7, J8, J13, CN6, Q1, R1, R2, C1, C2 |
| `1-Wire-probes.SchDoc` | 2 Sep, 13:17 | J1–J6, D1–D6, R3–R14 |
| `I2C-sensors.SchDoc` | 3 Sep, 15:31 | U3, J9–J12, R15–R23, R39, R40, C8 — and C4, C5, C6 |
| `Buck-regulator.SchDoc` | 3 Sep, 22:43 | F1, Q2, R38, D9, D10, C11, C19, C21, R24, R25, C10, the Traco module |

There is **no `.PcbDoc`**, and there is **no project library** — every part is
placed from `Miscellaneous Devices.IntLib` / `Miscellaneous Connectors.IntLib`
that shipped with the program, plus three downloaded libraries:
`TSR_1-2433.IntLib`, `TCA9548APWR.IntLib` and `CDSOD323-T05LC.SchLib`.

Cross-sheet connectivity is carried entirely by **power ports** — `VSENS`,
`DQ_P0`…`DQ_P5`, `I2C_SDA`, `I2C_SCL`, `SENS_GATE`, `VBAT_SENSE`, `GND`,
`24V_RAW`, `24V_PROT`. There is not one net label and not one port in the whole
project. That works — power ports are global regardless of net identifier scope —
but it has consequences, see §1 item 4.

The four live sheets are close to §4a. The gap between here and a board you can
order is: the audit below, a project library, one PCB document, and the outputs.

---

## 1. The audit — nine things that are wrong right now

Fix these before drawing a single track. Every one of them is silent: none stops
you from finishing the layout, and each one produces a board that is wrong in a way
you find out about after paying for it.

### 1. Every passive carries a through-hole footprint

| Refs | Footprint now | §4a says | Count |
|---|---|---|---:|
| R1–R25, R38, R39, R40 | `AXIAL-0.3` — an axial resistor on 0.3″ pitch | **0805** | ~30 |
| C1, C2, C4–C6, C8, C10, C21 | `RAD-0.3` — a radial cap on 0.3″ pitch | **0805** ceramic | 8 |

Push this to a PCB unchanged and you get a through-hole board with thirty axial
resistors on it. This is the single largest item on the list and §3 exists to fix it.

`C11` (100 µF ≥63 V electrolytic) and `C19` are the two that legitimately *are*
through-hole — but not `RAD-0.3`, see item 3.

### 2. Q1 and Q2 both carry `E3`

`E3` is the axial **diode** footprint. Both MOSFET symbols have three pins and
`E3` has two pads, so this one at least announces itself: the ECO into the PCB will
fail with a pin/pad mismatch. What it should be:

- **Q1 — AO3401A — SOT-23** (3 pads)
- **Q2 — DMP6023LE-13 — SOT-223** (3 pads plus the tab, and **the tab is the
  drain** — §5, *Why the drain faces the supply*. The tab is a fourth pad, not
  decoration, and it is the thermal path)

### 3. Four package errors on parts that do have a package specified

| Ref | Footprint now | Should be | Consequence |
|---|---|---|---|
| D9 (SMBJ33A) | `SMC` | **SMB** / DO-214AA | Pads too far apart; part sits on air |
| D10 (BZX84C12) | `SMC` | **SOT-23** (3 pads) — or SOD-123 if you fit MMSZ5242B | Two pads for a three-lead part |
| C19 | `RAD-0.3`, value "22uF 16v", comment "2.2 µF/100 V" | **22 µF / 50 V**, X7R 1210 or a small electrolytic | Three different answers on one part. §5 calls the 22 µF/50 V input cap *Traco's requirement, not a choice*, for V_in > 32 V |
| C11 | `RAD-0.3` | 100 µF **≥63 V** radial, ~10 mm body, 5 mm lead pitch | A 0.3″ radial land will not take the real capacitor |

### 4. The 3.3 V rail has three different names

`Buck-regulator.SchDoc` names the module output **`3.3V`**. `Connector.SchDoc`
uses **`U7 3v3`** and **`V3V3_MCU`**. Power ports connect by *name*, and those
three names are three different nets. **As drawn, the buck output does not reach
J8 or CN6.**

Pick one name, use it on both sheets. `V3V3_MCU` is the better name — it says
which rail it is, and it will not be confused with `VSENS`, which is the gated
sensor rail downstream of Q1 and is a genuinely different net.

While you are there: the schematic uses power ports for *signals* (`DQ_P0`,
`I2C_SDA`, `SENS_GATE`). It works, but a power port carries the "power object"
flag, which suppresses the ERC check for *"net has no driving source"*. Convert
the signal ones to **net labels** (`Place » Net Label`) and keep power ports for
`GND`, `V3V3_MCU`, `VSENS` and `24V_RAW`/`24V_PROT`. Then set the scope explicitly:
`Project » Project Options » Options` tab, **Net Identifier Scope = Flat**. It is
currently *Automatic*, which happens to resolve to Flat because there are no ports
in the design — but that is a coincidence you would rather not depend on.

### 5. The test points are power ports, not pads

`TP1`, `TP15`, `TP16`, `TP21`, `TP22`, `TP23`, `TP25`, `TP26` appear as power
ports. A power port names a net; it does not create anything you can touch with a
probe. §4a asks for **TP1–TP26**, twenty-six of them, and the reason is in §6:
*"With six probes and four I2C branches, 'which one?' is the first question of
every field failure."*

Worse, a `TP1` power port sitting on the same wire as a `VSENS` power port gives
that net two names, which Altium reports as a warning and then quietly picks one.

Delete the TP power ports. Test points are a **PCB** object: see §7 item 6.

### 6. C4, C5 and C6 are still there

§4a: *"Deleted from the previous revision: U2 (SCD41), C3, C7, and U1a/U1b/U1c with
C4–C6 — the SHT45s are no longer on this board."* The SHT45 decoupling now lives
out at each sensor head (§4b). Delete C4, C5, C6 from `I2C-sensors.SchDoc`.

### 7. Two components are unannotated

The Traco module is **`PS?`** and one 100 nF on the I2C sheet is **`C?`**. §4a calls
the module **U7** and every reference in §5 and §7 uses that name — `TP23`/`TP24`
are *"V_in and V_out of U7"*, and the bring-up steps name it. Rename it by hand.

Then, when you annotate: **`Tools » Annotate Schematics...`** and set *Order of
Processing* so it fills gaps only. **Do not use "Reset Designators".** §4a owns
every reference designator on this board, `sourcing-th.md` orders parts against
them, and §7's bring-up procedure names them one by one. A full reset renumbers
R3–R14 into some other order and every one of those documents becomes wrong at
once.

### 8. J1–J6 and J9–J12 are pin headers

They are `HDR1X3` and `HDR1X4` — 2.54 mm pin headers. §4a and `sourcing-th.md`
specify **Phoenix MC 1,5/3-ST-3,5** (1840379) and **MC 1,5/4-ST-3,5** (1840382),
pluggable screw terminals on **3.5 mm** pitch. §6 gives the reason: *"Field
re-termination with cold hands is the design case."*

The pitch difference alone (3.5 vs 2.54 mm) makes this a real footprint job, and
these ten connectors are what sets the size of the board — see §6.

### 9. The probe silkscreen names do not match the firmware

The nets are right: `DQ_P0`…`DQ_P5`, matching `DS_PROBE_BUSES` in
`include/node_config.h`. The **Comment** fields are not — J1–J6 read
`HOT_PROBE_1 / COLD_PROBE_1 / HOT_PROBE_2 / COLD_PROBE_2 / HOT_PROBE_3 /
COLD_PROBE_3`, which is a three-pair scheme that does not exist anywhere in the
firmware.

§6: *"number them `P0`–`P5` to match `DS_PROBE_BUSES` and the dashboard metric
names."* `temp_hot` and `temp_cold` are the names of the first two *slots*, kept for
backward compatibility with the packet format (`src/lora/lora_packet.h`), not the
names of two kinds of probe. Set the comments to `P0`…`P5`; they become the
silkscreen, and the silkscreen is what someone reads on a pole in the rain.

---

## 2. Setting up

### 2.1 Move the project into the repository

It currently lives in `C:\Users\Public\Documents\Altium\Projects\WaterTempNode_FE`,
which is outside the repo, so nothing in it is versioned and nothing cross-checks
against §4a.

1. **Close Altium.**
2. Move the whole folder to `water-temp-node/hardware/WaterTempNode_FE/`.
3. Delete the `History/` subfolder — it is Altium's own local backup and there are
   over a hundred `.Zip` files in it. Git is the history from now on.
4. Reopen by double-clicking `WaterTempNode_FE.PrjPcb`.

The document paths in the `.PrjPcb` are relative (`DocumentPath=FrontEnd.SchDoc`),
so the move is safe. The *library* paths are not — see §3.4.

### 2.2 `.gitignore`

Append to `water-temp-node/.gitignore`:

```gitignore
# Altium generated output and local state
hardware/**/History/
hardware/**/__Previews__/
hardware/**/Project Logs*/
hardware/**/Project Outputs*/
hardware/**/*.PrjPcbStructure
hardware/**/*.~*.Zip
hardware/**/*.SchDocPreview
hardware/**/*.PcbDocPreview
```

Commit the sources — `.PrjPcb`, `.SchDoc`, `.PcbDoc`, `.SchLib`, `.PcbLib`,
`.OutJob` — and commit the released fabrication zip only when you actually order a
board, so the tag and the gerbers travel together.

These are binary files. `git diff` will tell you nothing useful about them, so
**write real commit messages**: "add SOT-223 land for Q2, tab tied to drain" beats
"update PCB".

### 2.3 Take `FrontEnd.SchDoc` out of the project

It is `Document1` in the `.PrjPcb`, and it holds a full copy of the old board:
C1, C2, J1–J8, Q1, R1 — all of which now also exist on the new sheets. Compile the
project as it stands and you get a wall of duplicate-designator errors that hides
everything you actually want to read.

`Project » Remove from Project...`, select `FrontEnd.SchDoc`. **Keep the file on
disk** — rename it `FrontEnd-rev1-superseded.SchDoc`, matching how
`hardware-interface-back.md` and `hardware-interface-s88.md` keep superseded
revisions in this repo. It is the only drawn record of the board that had the
SHT45s and the SCD41 on it.

---

## 3. The project library

### 3.1 Why bother

Three of the parts already come from downloaded libraries that live somewhere on
this machine and are not in the repo. When those files move, the schematic keeps
working — the symbol is cached in the `.SchDoc` — but the **footprint link
breaks**, and it breaks at the moment you push changes to the PCB, which is the
moment you are least able to notice one missing part among sixty.

A project library fixes that: one `.SchLib` and one `.PcbLib`, committed next to
the sheets, containing everything this board uses.

### 3.2 Create them

`File » New » Library » Schematic Library` and `File » New » Library » PCB Library`.
Save them into the project folder as `WaterTempNode_FE.SchLib` and
`WaterTempNode_FE.PcbLib`. They join the project automatically.

### 3.3 There is no IPC wizard in this version

Newer Altium has an *IPC Compliant Footprint Wizard* that generates a land pattern
from package dimensions. **Altium Designer 17.0.11 does not have it** — the string
appears in no resource file and in no binary in the install. What you have is:

**`Tools » Component Wizard...`** in the PCB Library editor — a parametric wizard
covering resistors, capacitors, SOIC/SOP, headers and a few others. It asks for pad
size and spacing directly; it does not compute them from a package outline.

So the pad dimensions have to come from somewhere, and the right somewhere is the
**manufacturer's recommended land pattern**, which every part on this board
publishes. For hand soldering, take that recommended land and extend the *outer*
end of each pad by 0.2–0.3 mm — that is where the iron tip and the solder go. Do
not widen the gap between pads; that is what sets the part's self-alignment.

### 3.4 What to build, and from where

| Refs | Package | Where the land pattern comes from |
|---|---|---|
| R1–R25, R38–R40 | 0805 | Resistor datasheet, "recommended solder pad". One footprint, thirty uses |
| C1, C2, C8, C10, C21 | 0805 | Same; C21 may be 1210 if you buy 22 µF that way |
| C19 | 1210 or radial | 22 µF **50 V** — check which you can actually buy before drawing the land |
| C11 | Radial electrolytic | 100 µF ≥63 V. Measure the real part: body ~10 mm, lead pitch 5 mm |
| Q1 | SOT-23 | AOS AO3401A datasheet |
| Q2 | **SOT-223** | Diodes DMP6023LE datasheet. Four pads — **tab = drain** |
| D1–D6 | SOD-323 | Already have `CDSOD323_BRN` from the downloaded library — **copy it into the project `.PcbLib`** and verify against the Bourns datasheet rather than trusting it |
| D9 | SMB / DO-214AA | Littelfuse SMBJ series datasheet |
| D10 | SOT-23 or SOD-123 | Match whichever part you buy |
| U3 | TSSOP-24, 0.65 mm | `SOP65P640X120-24N` from the downloaded library is the correct package — copy it in and check the pad pitch against TI's PW package drawing |
| U7 | SIP-3, 2.54 mm | `CONV_TSR_1-2433` from the downloaded library. **Buzz it out before soldering** (§5): a reversed footprint puts 24 V on the output pin, and the Nucleo is downstream |
| F1 | 5×20 mm holder | Two pads at the holder's lead spacing — measure the holder you buy |
| J1–J6 | **MC 1,5/3-ST-3,5** | Phoenix drawing 1840379. 3 pads, **3.5 mm** pitch |
| J9–J12 | **MC 1,5/4-ST-3,5** | Phoenix drawing 1840382. 4 pads, 3.5 mm pitch |
| J7 | 2×19 shrouded header | 2.54 mm, 38 pads. Get the **shroud outline** onto the mechanical layer — it is much larger than the pin field, and it is what collides with things |
| J8, J13, J14 | 2/3/2-pin | Per `sourcing-th.md` |

Two footprints do the work of thirty-eight parts here (0805 and the two Phoenix
terminals), so start with those three and the board is more than half libraried.

### 3.5 Every footprint gets checked twice

Once when you draw it, against the datasheet. Once when the boards arrive, before
you solder anything: print the fabrication drawing **at 1:1** on paper and set the
real parts on it. It costs five minutes and it is the only check that catches a
footprint that is self-consistently wrong.

---

## 4. Fixing the schematic

Work through §1 in order. Two mechanical notes:

**Changing a footprint on many parts at once.** Select one resistor, right-click,
`Find Similar Objects...`, set *Library Reference* to `Same`, OK. The Inspector
panel opens with every resistor selected; change the footprint there once. This is
the difference between fixing thirty parts and fixing thirty parts thirty times.

**MPN parameters.** Add a parameter named `MPN` to the twelve parts that must not
be substituted — `Q1` AO3401A, `Q2` DMP6023LE-13, `D1`–`D6` CDSOD323-T05LC, `D9`
SMBJ33A, `D10` BZX84C12, `U3` TCA9548APWR, `U7` TSR 1-2433, `F1` (2 A time-lag,
I²t ≥0.5 A²s), and the four Phoenix connectors. The 0805 passives keep Value and
footprint only; they are commodities.

The reason to put these *in the schematic* rather than only in `sourcing-th.md` is
that the BOM is what someone reads a year from now while ordering, and
`sourcing-th.md` is explicit about what happens if they guess: *"do not reuse the
AO3401A, it is a 30 V part."*

### J7 needs 38 deliberate decisions

§2, *What to draw on J7 in the schematic*, is a table you follow position by
position: 11 positions carry a net, 5 get grounded, 22 get a **no-connect flag**.
Place them with `Place » Directives » Generic No ERC`.

Read that section before drawing it. Grounding CN10-7 shorts the ADC reference;
CN10-22 is the radio's TCXO supply; positions 26/28/30 are the Nucleo's user LEDs
and would sink current continuously if grounded. The flags are not cosmetic —
they are how you tell a deliberate non-connection from one you forgot, and with 22
of them on one connector you will not remember which is which in a month.

---

## 5. Compile

**`Project » Compile PCB Project WaterTempNode_FE.PrjPcb`.**

(In this version the command is *Compile*. Altium 18 renamed it *Validate*, so
tutorials written after 2018 use the other word for the same thing.)

Results land in the **Messages** panel. Zero errors is the target; warnings need
reading, not clearing. The ones that matter here:

- **Duplicate designator** — `FrontEnd.SchDoc` is still in the project (§2.3), or
  annotation has not been run
- **Net has no driving source** — usually a net that exists on one sheet only,
  which after §1 item 4 means a name that does not match its other half
- **Nets with multiple names** — two power ports on one wire; the `TP*` ports
- **Floating net label / unconnected pin** — a real missed connection, and the
  reason for putting no-connect flags on J7's 22 unused positions: without them
  this message is drowned

Double-click any message to jump to the object. Fix, save, compile again. Do not
start the PCB until this is clean — every error here becomes a harder error there.

---

## 6. Creating the PCB and sizing the board

### 6.1 The document

`File » New » PCB`, save as `WaterTempNode_FE.PcbDoc` in the project folder.

`Design » Board Options...` — set the grid to **1 mm** with a **0.1 mm** snap.
Press `Q` to toggle units if the display shows mils; this board is metric because
every datasheet you will read for it is.

`Design » Layer Stack Manager...` — the default is a 2-layer board, which is what
you want. Do not add layers.

### 6.2 The board outline is set by connectors, not by circuitry

Nothing in the spec fixes a board size, because nothing should: the size falls out
of eleven cable entries (§6). Do the arithmetic before drawing anything.

| Group | Each | Count | Edge length |
|---|---|---:|---:|
| J1–J6, MC 1,5/**3**-ST-3,5 @ 3.5 mm | ~11.5 mm | 6 | **~69 mm** |
| J9–J12, MC 1,5/**4**-ST-3,5 @ 3.5 mm | ~15 mm | 4 | **~60 mm** |
| J14 (24 V in) | ~10 mm | 1 | 10 mm |
| J8, J13 | ~8 mm | 2 | 16 mm |

All ten terminals on one edge is ~130 mm of connector, which is a silly board. Put
the **six probes along one long edge** and the **four I2C branches along the
opposite long edge**; J14 with U7 at one short edge; J8 and J13 at the other.
That gives roughly **80 × 60 mm**, and it satisfies §5's placement rule 2 —
*"the module in one corner, with the 24 V input, far from J1–J12"* — because the
24 V corner is then diagonally away from every sensor terminal.

Draw it: `Design » Board Shape » Redefine Board Shape`, or draw a closed outline on
the **Mechanical 1** layer and use `Design » Board Shape » Define from selected
objects`. Add four **M3 mounting holes**, 3.2 mm, inset ~5 mm from the corners —
§4a specifies M3 nylon standoffs for both boards.

### 6.3 Design rules

`Design » Rules...`. The defaults are for a much finer board than you need; a
2-layer JLCPCB board with hand-soldered parts should be deliberately coarse,
because coarse is what survives.

| Rule | Value | Why |
|---|---|---|
| Clearance (default) | **0.25 mm** | JLCPCB's 2-layer minimum is 0.127 mm. Nothing here needs to go near that |
| Clearance, 24 V net class | **0.5 mm** | D9 clamps as high as **53.3 V** (§5). Electrically 0.25 mm would pass; this is margin against a surge, and it is free on a board with this much empty space |
| Width (default) | 0.25 mm, min 0.2, max 2 | Signals |
| Width, `V3V3_MCU` / `VSENS` | **0.5 mm** | The SCD41's 205 mA burst plus everything else on the gated rail |
| Width, `24V_RAW` / `24V_PROT` / GND return to U7 pin 2 | **1.0 mm** | F1 is a 2 A fuse; 1 mm on 1 oz outer copper carries that with a small rise. §5 rule 3: *"Give pin 2 its own wide GND return to J14"* |
| Routing Via Style | 0.3 mm hole / 0.6 mm pad | Comfortably above JLCPCB's 0.3/0.45 minimum |
| Silkscreen to solder mask | 0.15 mm | Keeps reference designators off pads |
| Polygon connect style | **Relief** on through-hole pads | Direct connect to a ground pour makes a THT joint that a hand iron cannot heat. This one rule decides whether the board is solderable |

Add a **net class** for the 24 V section (`Design » Classes...`) so those two rules
have something to attach to.

---

## 7. Placement

`Design » Import Changes From WaterTempNode_FE.PrjPcb` pulls the components across.
Validate, then Execute. Everything lands in a heap outside the outline; that is
normal.

Place in this order. Placement is where this board is won or lost — the routing is
easy once the parts are in the right places, and impossible if they are not.

1. **The connectors first, on the outline, per §6.2.** They are fixed by the
   enclosure and the cable entries; everything else moves around them. Lock them
   (`Edit » Lock`) once positioned.
2. **U7 and the 24 V input in their corner.** §5's four rules, in full:
   - `C19` across pins 1 and 2, **within 5 mm**
   - the module in one corner with the 24 V input, far from J1–J12
   - pin 2 gets its own wide GND return to J14 — not shared with analog ground or R25
   - **nothing routed under the module**; the body sits 0.5 mm off the board, so
     keep that area copper-free on the top layer. Draw a keep-out on the
     mechanical layer so you cannot forget
3. **F1 → Q2 → D9 → C11 in that physical order** along the 24 V path. §5 is
   explicit that the order matters, and it is the order of the sheet.
4. **D1–D6 hard against J1–J6.** A TVS placed away from its connector protects the
   trace instead of the chip. Same for R9–R14, the series resistors — connector,
   then TVS, then series R, then off toward J7.
5. **U3 between the I2C connectors and J7**, with C8 against its supply pins. The
   four downstream pull-up pairs (R17–R22, R39, R40) go near their connectors, not
   near U3 — they are terminating 5 m of cable each.
6. **Test points last, TP1–TP26** (§4a). Use a 1.5 mm pad with no component: place
   a free pad, or make a one-pad "TP" footprint in the project library and place it
   as a real component so it appears in the BOM count. Label every one on the
   silkscreen. TP15–TP22 are the four downstream SDA/SCL pairs, and §6 is blunt
   about why: *"Without these, a dead sensor and a dead mux channel look
   identical."*

---

## 8. Routing, then copper

Route the top layer, and treat the bottom as ground that you are not allowed to cut.

Order: the 24 V section first (widest, least flexible), then the 3.3 V rails, then
signals. Interactive routing is `Place » Interactive Routing`; `Shift+Space`
cycles corner style; `*` on the numeric keypad drops a via and swaps layers.

**The bottom layer is a solid ground plane.** Every track you route on it cuts the
return path of whatever runs above. On a board with a 500 kHz switching regulator
and a 27 kΩ ADC divider on it, that is the mechanism by which a working board
becomes a board with noisy readings. If a signal cannot be routed on top, ask
whether the placement is wrong before you cut the plane.

Then the pours:

1. `Place » Polygon Pour`, over the whole board on **Bottom Layer**, net `GND`,
   solid, remove dead copper.
2. The same on **Top Layer**, net `GND`, in whatever space the tracks leave.
3. **Stitching vias** between them — one every ~10 mm across open areas, and
   specifically a cluster near U7's GND pin and near J14. Without stitching the top
   pour is a collection of isolated islands, which is worse than no pour.
4. `Tools » Polygon Pours » Repour All` after any routing change. A stale pour is
   the classic way to ship a board with an unconnected ground.

---

## 9. DRC

**`Tools » Design Rule Check...`** — run the full check, not the online one.

Expect and fix: clearance violations, unrouted nets, silkscreen over pads, and
short-circuits between a pour and a net you forgot to assign. **Unrouted Net** count
must be zero, and read the number rather than the colour: a single unrouted
connection on GND is invisible on a board covered in ground pour.

Then look at it in 3D (`View » 3D Layout Mode`, or `3`) — not for beauty, but
because connector shrouds and the Traco module's body are the two things that
collide with an enclosure, and 3D is where you see it.

---

## 10. Output files

`File » Fabrication Outputs`, or better, put them all in the `.OutJob` that already
exists in the project so the settings are recorded and repeatable.

### Gerber

`Gerber Files`:
- **Units: millimetres, format 4:4**
- Layers to plot: Top Layer, Bottom Layer, Top Overlay, Bottom Overlay (if used),
  Top Solder Mask, Bottom Solder Mask, Top Paste (only if you order a stencil),
  and **Mechanical 1** as the board outline
- Tick **"Include unconnected mid-layer pads"** off — 2-layer board, irrelevant
- On the *Advanced* tab, leave **"Embedded apertures (RS274X)"** on

### NC drill

`NC Drill Files` — same units and format as the Gerbers. **4:4 metric in both, or
your holes land somewhere other than your pads.** This is the single most common
way a first board comes back wrong.

### BOM

`Report » Bill of Materials`. Columns: Designator, Comment, Value, Footprint,
Quantity, **MPN**. Export as `.csv` or `.xlsx` into the project folder.

Then check it against [`parts-list-th.md`](parts-list-th.md) — that file is the
authority on *how many* and [`sourcing-th.md`](sourcing-th.md) on *what may be
substituted*. If the Altium BOM and `parts-list-th.md` disagree on a count, one of
them is wrong and finding out which is the whole point of generating both.

### Assembly drawing

Print the top overlay with designators at **1:1** on paper. You are hand-soldering
five boards from a bag of 0805 parts that all look identical; this is the map.

### What to send

Zip the Gerbers and the NC drill together — no folders, no extra files — and
upload to JLCPCB. Their viewer renders every layer: **look at it**. Board outline
present, holes in pads, silkscreen legible, no copper where you did not put any.
Then order **5 boards, 1.6 mm, 1 oz copper, HASL** (HASL over ENIG here — hand
soldering, and HASL is cheaper).

---

## 11. Before you spend money

- [ ] `FrontEnd.SchDoc` removed from the project, kept on disk as
      `FrontEnd-rev1-superseded.SchDoc`
- [ ] Compile: **zero errors**, every warning read
- [ ] No `AXIAL-0.3` or `RAD-0.3` anywhere except the two parts that really are
      through-hole (C11, and C19 if you bought it radial)
- [ ] Q2 is SOT-223 with **the tab on the drain net**
- [ ] One name for the 3.3 V rail, on both sheets
- [ ] C4, C5, C6 deleted; `PS?` renamed U7; `C?` annotated
- [ ] J7: 11 nets, 5 grounds, **22 no-connect flags** — against §2's table, position
      by position
- [ ] TP1–TP26 exist as **pads**, all labelled
- [ ] J1–J6 silkscreened `P0`–`P5`; J9–J12 silkscreened `CH0 หัว`, `CH1 ท้าย`,
      `CH2 นอก`, `CH3 CO2 กลาง` (§4a)
- [ ] Nothing routed under U7; C19 within 5 mm of its pins 1–2
- [ ] Pour repoured after the last routing change; stitching vias placed
- [ ] DRC clean, **Unrouted Net = 0**
- [ ] Gerber and NC drill both metric 4:4
- [ ] Footprints printed at 1:1 and checked against real parts
- [ ] BOM reconciled against `parts-list-th.md`

Then, when the boards arrive, §7 of `hardware-interface.md` is the bring-up order —
including step 22, which needs an oscilloscope at the CO2 head.

---

## References

- [`hardware-interface.md`](hardware-interface.md) — the spec. §2 for J7, §3 for the
  front-end and the SCD41, §4a/§4b for the BOM, §5 for power and U7's layout rules,
  §6 for cables and enclosures, §7 for bring-up
- [`sourcing-th.md`](sourcing-th.md) — manufacturer part numbers and substitution rules
- [`parts-list-th.md`](parts-list-th.md) — quantities, spares, purchase waves
- Manufacturer land patterns: Phoenix 1840379 / 1840382, AOS AO3401A, Diodes
  DMP6023LE, Bourns CDSOD323, TI TCA9548A (PW), Traco TSR 1 series, Littelfuse SMBJ

**Version note.** Every menu path in this document was read out of the resource
files of the installed copy — Altium Designer **17.0.11 (build 656)**. Later
versions moved several of them: *Compile* became *Validate*, the Layer Stack
Manager became a document, and the IPC Compliant Footprint Wizard was added. If you
upgrade, this file needs a pass.
