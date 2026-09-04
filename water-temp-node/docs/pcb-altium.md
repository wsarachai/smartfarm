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

**Three sheets**, in `C:\Users\Public\Documents\Altium\Projects\WaterTempNode_FE`:

| Sheet | What is on it |
|---|---|
| `FrontEnd-signals.SchDoc` | J7, J13, **CN6**, Q1, R1, R2, C1, C2 **and** J1–J6, D1–D6, R3–R14 |
| `I2C-sensors.SchDoc` | U3, J9–J12, R15–R23, R39, R40, C8 |
| `Buck-regulator.SchDoc` | F1, Q2, R38, D9, D10, C11, C19, C21, R24, R25, C10, **U7** |

It was four sheets until 4 September, when `Connector.SchDoc` was merged into the
probe sheet and the combined sheet was renamed `FrontEnd-signals` — the old name
stopped describing a sheet that also carries the signal connector, the rail gate
and the debug port. `Connector.SchDoc` is out of the project and off the disk;
`History\` still holds 43 copies of it if any of that turns out to be needed.

**J8 is gone too**, deleted the same day. The 3.3 V output now leaves through
`CN6`, a board-side 1×8 header mirroring the Nucleo's — see §2 of the spec, *The
CN6 power tap*, and key that lead.

A fifth sheet, `FrontEnd.SchDoc`, held the whole **pre-revision-2.0 board** — 58
refs including U1a/U1b/U1c, U2 and TVS1, from when the SHT45s and the SCD41 were
still on the PCB. **It was deleted from disk on 4 September.** Two consequences,
both live right now:

- The `.PrjPcb` was last written before the deletion and **still lists it** as
  `[Document1]`. Deleting a file does not remove it from the project; Altium will
  report a missing document until the entry goes too (§2.3).
- It is not lost yet. `History\` still holds **49 `FrontEnd*` backups (~1 MB)**,
  which is the only surviving copy of that drawing — and §2.2 tells you to delete
  `History\`. Recover it *first* (§2.1 step 2).

There is **no `.PcbDoc`**. Generic parts come from `Miscellaneous Devices.IntLib` /
`Miscellaneous Connectors.IntLib`, which shipped with the program. Everything
specific comes from **`C:\Users\ASUS\Documents\Altium`**:

| Folder | Used by | State |
|---|---|---|
| `TSR_1_2433\TSR_1-2433.IntLib` | U7 | In use — `CONV_TSR_1-2433` |
| `TCA9548APWR.IntLib` | U3 | In use — `SOP65P640X120-24N` |
| `CDSOD323-T05LC\` (`.SchLib` + `.PcbLib` + STEP) | D1–D6 | In use — `CDSOD323_BRN` |
| `DMP6023LE-13\` (`.SchLib` + `.PcbLib` + STEP) | **Q2** | **Downloaded but never used** — Q2 is still a generic `MOSFET-P` with an `E3` footprint |
| `LM5164DDAR\` | — | **Stale.** The LM5164 buck was deleted in revision 1.0 (§4a). Nothing refers to it |

The same folder also holds `mb1389_bdp\` — ST's own Altium source for the
**NUCLEO-WL55JC1 (MB1389)**, schematic and PCB. That is a better authority than
UM2592 for anything about CN10 or CN6, and §2 of the spec depends on getting those
right.

Cross-sheet connectivity is carried entirely by **power ports** — `VSENS`,
`DQ_P0`…`DQ_P5`, `I2C_SDA`, `I2C_SCL`, `SENS_GATE`, `VBAT_SENSE`, `GND`,
`24V_RAW`, `24V_PROT`. There is not one net label and not one port in the whole
project. That works — power ports are global regardless of net identifier scope —
but it has consequences, see §1 item 4.

The four live sheets are close to §4a. The gap between here and a board you can
order is: the audit below, a project library, one PCB document, and the outputs.

---

## 1. The audit — nine things that were wrong

Fix these before drawing a single track. Every one of them is silent: none stops
you from finishing the layout, and each one produces a board that is wrong in a way
you find out about after paying for it.

> **Progress as of 4 September, 20:07**, read from the sheets themselves:
>
> | Item | State |
> |---|---|
> | 1 — through-hole footprints on the passives | **done** |
> | 2 — Q1/Q2 on `E3` | **done** — Q1 `SOT23-3-M`, Q2 `DMP6023LE-13_DIO-M` |
> | 3 — package errors | **partly** — D9 is `SMB-DO214AA-M`; **C11 and C19 are still `RAD-0.3`** |
> | 4 — three names for the 3.3 V rail | **done**, `V3V3_MCU` ×3 |
> | 5 — test points are power ports | **not yet** — all eight still power ports |
> | 6 — C4, C5, C6 | **done** |
> | 7 — `PS?` and `C?` unannotated | **done** |
> | 8 — pin headers where terminals belong | **half** — the new footprints are on, **the old ones were never removed** |
> | 9 — probe silkscreen names | **wrong, not merely undone** — they read `P1`–`P6`, and the nets on the same sheet read `DQ_P0`–`DQ_P5` |
>
> Also clean, and checked directly: **no orphaned wires on any sheet**, **no
> floating power ports**, J7's symbol is now `Header 20X2` with 40 pins to match
> `HDR2X20-BOX`, and `no-connect` is no longer a power port.
>
> **Two things are silent and worth repeating.**
>
> *Stale footprint models.* Adding a footprint does not remove the old one, and a
> part with two models gives no warning about which is current. Seven `HDR1X3`
> models are still attached to J1–J6 and J13, and D10 still carries `SOT-23`
> alongside `SOT23-3-M`. **Remove**, do not just **Add**.
>
> *NoERC directives went from 14 to 2 when the sheets were merged.* §2 needs
> **21** on J7. Nothing reports their absence — the pins simply read as
> not-yet-wired, mixed in with every other warning, which is the exact outcome §2
> put the flags there to prevent.

## 2. Setting up

### 2.1 Move the project into the repository

It currently lives in `C:\Users\Public\Documents\Altium\Projects\WaterTempNode_FE`,
which is outside the repo, so nothing in it is versioned and nothing cross-checks
against §4a.

1. **Close Altium.**
2. **Rescue the old sheet before anything else.** In `History\`, take the
   **newest** `FrontEnd.~(nn).SchDoc.Zip`, unzip it, and save the result as
   `hardware/FrontEnd-rev1-superseded.SchDoc`. Do **not** add it to the project —
   it is an archive, and putting it back re-creates the duplicate-designator
   problem it was deleted to solve. This repo keeps superseded revisions
   (`hardware-interface-back.md`, `hardware-interface-s88.md`) but keeps them as
   *prose*; this is the only drawn record of the board that carried the SHT45s and
   the SCD41 on it.
3. Move the whole project folder to `water-temp-node/hardware/WaterTempNode_FE/`.
4. **Copy the part libraries in too.** From `C:\Users\ASUS\Documents\Altium`, copy
   `TSR_1_2433\`, `TCA9548APWR.IntLib`, `CDSOD323-T05LC\` and `DMP6023LE-13\` into
   `water-temp-node/hardware/lib/`. Take the STEP models with them. **Leave
   `LM5164DDAR\` behind** — that part was deleted in revision 1.0 — and leave
   `mb1389_bdp\` behind as well: ST's Nucleo design is 30 MB of reference material
   that this board does not build.
5. *Now* delete the `History\` subfolder. Git is the history from here.
6. Reopen by double-clicking `WaterTempNode_FE.PrjPcb`.

The document paths in the `.PrjPcb` are relative (`DocumentPath=I2C-sensors.SchDoc`),
so the move is safe. Library paths are not — see §3.2.

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
`.OutJob` — **and commit `hardware/lib/`**, vendor libraries and STEP models
included. They are a few hundred kilobytes and they are the difference between a
repo that builds a board and a repo that describes one. Commit the released
fabrication zip only when you actually order a board, so the tag and the gerbers
travel together.

These are binary files. `git diff` will tell you nothing useful about them, so
**write real commit messages**: "add SOT-223 land for Q2, tab tied to drain" beats
"update PCB".

### 2.3 Clear the stale `FrontEnd.SchDoc` entry

The file is gone; the *project entry* is not. `WaterTempNode_FE.PrjPcb` still
carries `[Document1] DocumentPath=FrontEnd.SchDoc`, so Altium opens the project
reporting a missing document, and it will keep doing so until the entry is
removed.

`Project » Remove from Project...`, select `FrontEnd.SchDoc`, and save the project.
That rewrites the `.PrjPcb` and the warning goes.

Deleting the file did fix the real problem, which was worth stating: it held a full
copy of the old board — C1, C2, J1–J8, Q1, R1 — all of which also exist on the four
current sheets, so compiling with it in the project produced a wall of duplicate
designators that buried every message worth reading.

---

## 3. The project library

### 3.1 Why bother

Four of the parts come from downloaded libraries that lived in
`C:\Users\ASUS\Documents\Altium` — outside the repo, on one machine. When those
files move, the schematic keeps working, because the symbol is cached in the
`.SchDoc`. The **footprint link breaks**, and it breaks at the moment you push
changes to the PCB: the moment you are least able to notice one missing part among
sixty.

§2.1 step 4 copies them into `hardware/lib/`. That solves *where the files are*.
The second half is *how the project finds them*, and it is the part that is easy to
get wrong.

### 3.2 Add them to the project, do not install them globally

Altium has two ways to make a library available, and they store the path
differently:

| | Where it is set | Path stored | Survives cloning the repo? |
|---|---|---|---|
| **Installed (global)** | Libraries panel » *Libraries…* » Installed | **Absolute** — `C:\Users\ASUS\…` | **No** |
| **Project library** | `Project » Add Existing to Project...` | **Relative to the `.PrjPcb`** | **Yes** |

Use the second. Add all four — `TSR_1-2433.IntLib`, `TCA9548APWR.IntLib`,
`CDSOD323-T05LC.PcbLib` + `.SchLib`, `DMP6023LE-13.PcbLib` + `.SchLib` — with
`Project » Add Existing to Project...`. They then appear under the project in the
Projects panel and travel with it.

Then make one more pair for the parts nobody publishes a library for:
`File » New » Library » Schematic Library` and `... » PCB Library`, saved into the
project folder as `WaterTempNode_FE.SchLib` and `WaterTempNode_FE.PcbLib`. That is
where the 0805 land, the two Phoenix terminals and the rest of §3.6 go.

### 3.3 There is no IPC wizard in this version

Newer Altium has an *IPC Compliant Footprint Wizard* that generates a land pattern
from package dimensions. **Altium Designer 17.0.11 does not have it** — the string
appears in no resource file and in no binary in the install. What you have is:

**`Tools » Component Wizard...`** in the PCB Library editor — a parametric wizard
covering resistors, capacitors, SOIC/SOP, headers and a few others. It asks for pad
size and spacing directly; it does not compute them from a package outline.

So for anything you draw yourself, the pad dimensions have to come from the
**manufacturer's recommended land pattern**, which every part on this board
publishes. For hand soldering, take that recommended land and extend the *outer*
end of each pad by 0.2–0.3 mm — that is where the iron tip and the solder go. Do
not widen the gap between pads; that is what sets the part's self-alignment.

### 3.4 For the downloaded libraries, that work is already done — pick `-M`

Open `CDSOD323-T05LC.PcbLib` and there are three footprints, not one:

| Suffix | IPC-7351 density | Use it when |
|---|---|---|
| `-L` | **L**east — smallest lands | Fine-pitch machine assembly, tight boards |
| *(none)* | **N**ominal | The default, reflow assembly |
| `-M` | **M**ost — largest lands | **Hand soldering**, rework, prototypes |

`DMP6023LE-13.PcbLib` has the same three. The schematic currently uses the
**nominal** variant of both (`CDSOD323_BRN`, and `E3` for Q2 which is not even
close). This board is hand-soldered in a batch of five, so switch them:

- D1–D6 → **`CDSOD323_BRN-M`**
- Q2 → **`DMP6023LE-13_DIO-M`** (arriving with the vendor symbol, per §1 item 2)

That is the same margin the 0.2–0.3 mm rule above buys you, computed by someone
with the package drawing in front of them. `TCA9548APWR.IntLib` and
`TSR_1-2433.IntLib` ship one footprint each, so there is no choice to make there.

### 3.5 What you already have

Four parts are done. Verify each against the datasheet once — a downloaded
footprint is a claim, not a fact — and then never think about them again.

| Refs | Library | Footprint to use | Check |
|---|---|---|---|
| D1–D6 | `CDSOD323-T05LC` | **`CDSOD323_BRN-M`** | SOD-323 against the Bourns drawing |
| Q2 | `DMP6023LE-13` | **`DMP6023LE-13_DIO-M`** | SOT-223, four pads, **tab = drain** |
| U3 | `TCA9548APWR.IntLib` | `SOP65P640X120-24N` | TSSOP-24 pitch against TI's PW drawing |
| U7 | `TSR_1-2433.IntLib` | `CONV_TSR_1-2433` | **Buzz it out before soldering** (§5): a reversed footprint puts 24 V on the output pin, and the Nucleo is downstream |

### 3.6 What the script draws, and what is left

**Ten footprints are written as a script**,
[`hardware/scripts/MakeFootprints.pas`](../hardware/scripts/MakeFootprints.pas).
To run it:

1. `File » Open...` →
   [`hardware/scripts/WaterTempNode_Scripts.PrjScr`](../hardware/scripts/WaterTempNode_Scripts.PrjScr).
   **Altium will not run a loose `.pas`.** Its Run Script browser filters for
   *Script Project Files (\*.PrjScr)*, and the list of runnable scripts is built
   from open projects and open free documents — never from a path on disk. That
   project file exists only to make this script visible.
2. Open `WaterTempNode_FE.PcbLib` and make it the **active** document. The
   script writes into whichever PCB library is in front.
3. **`DXP » Run Script...`** — the `DXP` menu is the leftmost item on the menu
   bar, before `File`, and `Run Script...` is the last entry in it. It is **not**
   under `Tools` in this version. Pick `MakeFootprints.pas` › `MakeFootprints`.
Every dimension in it is quoted from the manufacturer document it came from, in a
comment above the line that uses it:

| Footprint | Covers | Land from |
|---|---|---|
| `CHIP0805-M` | R1–R25, R38–R40, C1, C2, C8, C10, C21 — **35 parts** | Vishay doc 28950, IPC-7351 reflow |
| `SOT23-3-M` | Q1, **D10** | AOS PO-00001 rev N |
| `SMB-DO214AA-M` | D9 | Bourns SMBJ, *Recommended Footprint* |
| `PHX-MC15-2-G-35` | **J14** | Phoenix 1844223 drilling plan |
| `PHX-MC15-3-G-35` | J1–J6 | same |
| `PHX-MC15-4-G-35` | J9–J12 | same |
| `RADIAL-D8-P35` | **C11 and C19** | Nichicon CAT.8100M case + lead tables |
| `HDR2X20-BOX` | **J7** | 2.54 mm grid is the definition of the part |
| `FUSEHOLDER-5X20-P226` | **F1** | 22.6 mm is the de-facto pitch for covered 5×20 PCB holders (PTF-78 family; Schurter OG `0031.8001` is the same class) |
| `HDR1X8-P254` | **CN6** — the 3.3 V output since J8 was deleted | 2.54 mm grid |
| `HDR1X3-P254` | **J13** | 2.54 mm grid |
| `TESTPAD-1MM5` | TP1–TP26 | — |

Every SMD land is the recommended one with each pad's **outer** end extended by
0.25 mm for hand soldering; the gap between pads is never widened, because that gap
is what makes the part self-align.

Five numbers are choices rather than citations and the script says so at each: the
2.2 mm pad on the Phoenix headers, the 0.9/1.8 mm hole and pad on the electrolytic,
the 1.0/1.8 mm on J7, the **1.5/3.0 mm on the fuse holder — oversized on purpose**,
because nobody in that class publishes a terminal thickness and an oversized hole
costs a little solder while an undersized one costs a board, and the 1.5 mm test
pad. Two **outlines** are provisional and both decide board area: **J7's shroud**
and **F1's body**.

Running a script does not excuse §3.7. Check what it made.

**Nothing is left to draw.** J8 was the last open item and it no longer exists —
the 3.3 V leaves through CN6 now, and CN6's header is in the list above. That lead
needs its key: **clip CN6-1 and plug hole 1** (§2 of the spec, *The CN6 power
tap*). Without it a reversed lead drives 3.3 V into the Nucleo's `5V` pin and holds
`NRST` at ground, which presents as a dead board rather than as a reversed cable.

### 3.7 Every footprint gets checked twice

Once when you draw it, against the datasheet. Once when the boards arrive, before
you solder anything: print the fabrication drawing **at 1:1** on paper and set the
real parts on it. It costs five minutes and it is the only check that catches a
footprint that is self-consistently wrong.

---

## 4. Fixing the schematic

Work through §1 in order. Two mechanical notes:

### Changing a footprint on many parts at once

Use **`Tools » Footprint Manager...`** from a schematic sheet. It lists every
component **in the whole project**, not just the open sheet, and it finishes with
an ECO you can read before anything changes. That is the difference between fixing
thirty parts and fixing thirty parts thirty times.

The reliable way to swap a footprint in bulk is **add the new one, then remove the
old one** — never "edit the name in place". A component with exactly one footprint
model has no ambiguity about which one is current, and that is the state you want
to end in.

1. Sort the grid by the **`Current Footprint`** column. Everything wrong is now
   grouped: one block of `AXIAL-0.3`, one of `RAD-0.3`, one of `HDR1X3`, one of
   `HDR1X4`.
2. Select the whole block (click the first row, shift-click the last).
3. In the footprint pane on the right, **Add** the new footprint, browsing to
   `WaterTempNode_FE.PcbLib`. It is added to every selected component at once.
4. With the same rows still selected, pick the **old** footprint in that pane and
   **Remove** it.
5. Repeat per block. Four passes cover forty-five parts:

| Sorted block | Add | Then remove |
|---|---|---|
| `AXIAL-0.3` — R1–R25, R38–R40 | `CHIP0805-M` | `AXIAL-0.3` |
| `RAD-0.3` — **C1, C2, C8, C10, C21 only** | `CHIP0805-M` | `RAD-0.3` |
| `HDR1X3` — J1–J6 | `PHX-MC15-3-G-35` | `HDR1X3` |
| `HDR1X4` — J9–J12 | `PHX-MC15-4-G-35` | `HDR1X4` |

**`C11` and `C19` are also `RAD-0.3` and must not be swept up in pass 2.** They
are the two capacitors that really are through-hole (§1 item 3), and their
footprints do not exist yet because they depend on which parts you buy.

6. **`Accept Changes (Create ECO)`** at the bottom. Read the change list, then
   *Validate Changes*, then *Execute Changes*. If a row reports an error rather
   than a tick, stop and read it — that is the ECO telling you a footprint does
   not exist or a pin does not map.

**Four parts are not bulk work**, because the symbol and the land do not line up
by themselves. Do these one at a time with the datasheet open:

- **Q2** — replace the whole component with the one from `DMP6023LE-13`, not just
  its footprint (§1 item 2)
- **Q1** — `SOT23-3-M`, after checking AO3401A's pin order against the generic
  `MOSFET-P` symbol's pin numbers
- **D10** — `SOT23-3-M`. A two-pin diode symbol onto a three-pad land needs an
  explicit pin/pad map, with the unused pad left unmapped
- **D9** — `SMB-DO214AA-M`. Two pins onto two pads, but confirm that pad 1, the
  cathode, is the symbol's cathode pin. D9 is unidirectional and only works one
  way round

**MPN parameters.** Add a parameter named `MPN` to the twelve parts that must not
be substituted — `Q1` AO3401A, `Q2` DMP6023LE-13, `D1`–`D6` CDSOD323-T05LC, `D9`
SMBJ33A, `D10` BZX84C12, `U3` TCA9548APWR, `U7` TSR 1-2433, `F1` (2 A time-lag,
I²t ≥0.5 A²s), and the four Phoenix connectors. The 0805 passives keep Value and
footprint only; they are commodities.

The four parts that come from vendor libraries — D1–D6, Q2, U3, U7 — often arrive
with a part-number parameter already, under some name of the library author's
choosing. Look before you add a second one: two parameters holding the same number
under different names is how a BOM ends up with an empty MPN column.

The reason to put these *in the schematic* rather than only in `sourcing-th.md` is
that the BOM is what someone reads a year from now while ordering, and
`sourcing-th.md` is explicit about what happens if they guess: *"do not reuse the
AO3401A, it is a 30 V part."*

### J7 needs 40 deliberate decisions

§2, *What to draw on J7 in the schematic*, is a table you follow position by
position: **14** positions carry a net, 5 get grounded, **21 get a no-connect flag**
(J7 is 2×20 and CN10 is 2×19, so positions 39 and 40 exist on the board and on
the cable but nowhere on the Nucleo).
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
  reason for putting no-connect flags on J7's 21 unused positions: without them
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
| CN6, J13 | ~22 mm / ~8 mm | 2 | 30 mm |

All ten terminals on one edge is ~130 mm of connector, which is a silly board. Put
the **six probes along one long edge** and the **four I2C branches along the
opposite long edge**; J14 with U7 at one short edge; CN6 and J13 at the other.
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

- [ ] `FrontEnd.SchDoc` recovered from `History\` and archived as
      `hardware/FrontEnd-rev1-superseded.SchDoc` **before** `History\` was deleted
- [ ] The stale `[Document1] FrontEnd.SchDoc` entry removed from the `.PrjPcb`
- [ ] Vendor libraries in `hardware/lib/`, attached with
      `Project » Add Existing to Project...` — **not** installed globally
- [ ] `LM5164DDAR` not copied in; `mb1389_bdp` not copied in
- [ ] Q2 is the **DMP6023LE-13 component**, not a generic `MOSFET-P`
- [ ] `-M` footprint variants selected: `CDSOD323_BRN-M`, `DMP6023LE-13_DIO-M`
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
