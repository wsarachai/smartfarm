# Etching `STM32WL_PT` at home — double-sided, toner transfer

The process document for the board specified in
[`hardware-interface-proto.md`](hardware-interface-proto.md). That file says *what*
the board is; this one says *how it gets made*, in the order it gets made, with the
places it goes wrong marked.

**This document decides nothing electrical.** Values, part numbers and placement
rules come from `hardware-interface-proto.md` and from
[`hardware-interface.md`](hardware-interface.md). What this file owns is design
rules that exist because of the *process*, and the order of operations.

**It is written to be worked through one stage at a time**, and every stage ends
with a **GATE** — a check that must pass before the next stage starts. The gates
are not ceremony. Three of them (§5, §7, §12) are the only points where a mistake
is still cheap to undo.

---

## Stage 0 — Materials and tools

Nothing here is exotic, but three items are worth not substituting.

| | Item | Notes |
|---|---|---|
| ☐ | **Double-sided FR4**, 1.6 mm, 1 oz (35 µm) copper | ≥ **180 × 140 mm** for a 160 × 120 board — the extra 10 mm all round carries the registration holes and gives something to hold |
| ☐ | **Laser printer** | Toner, not inkjet. Turn off toner-save and any "fit to page" |
| ☐ | Transfer paper | Glossy magazine paper, photo paper, or yellow transfer film. Whatever you have used before — this is not the stage to experiment |
| ☐ | **Laminator** (preferred) or clothes iron | A laminator is repeatable; an iron is a skill |
| ☐ | Etchant | FeCl₃, sodium persulfate, or HCl + H₂O₂ |
| ☐ | **Liquid tin** (immersion tin) | **Do not skip.** Stage 9 explains why |
| ☐ | **0.5 mm tinned copper wire** | Via wire. ~1 m covers ~30 signal vias plus 8–12 pour stitches |
| ☐ | Drill bits **0.8 / 1.0 / 1.1 / 1.3 mm** | Carbide if you can; HSS will do ~100 holes in FR4 before it stops cutting cleanly, and there are ~250. Buy spares of 1.0 mm — it does two thirds of the work |
| ☐ | Drill press or rotary tool **in a stand** | Freehand at 2.54 mm pitch does not work |
| ☐ | **2–3 pins**, 1.0 mm | Spare drill bits work. Registration |
| ☐ | 600–1000 grit paper or Scotch-Brite, IPA | Surface prep and flux removal |
| ☐ | Acetone | Toner strip |
| ☐ | Permanent marker | Touch-up of transfer breaks |
| ☐ | Multimeter with continuity, magnifier | Stage 7 and Stage 12 |
| ☐ | Conformal coating + masking tape | Stage 14 |
| ☐ | Small clip-on TO-220 heatsink | For Q3 — see `hardware-interface-proto.md` §4.2 |

---

## Stage 1 — Design rules, before any routing

Set these in Altium before the first track. They are loose by fab-house standards
and that is the point: board area is free here, yield is not.

| Rule | Value | Why this number |
|---|---|---|
| Track width, signal | **0.5 mm** | Toner transfer resolves ~0.3 mm on a good day. 0.5 mm survives a pinhole and a marker touch-up |
| Track width, `24V_PROT` / `24V_PRE` / GND return | **3 mm** | D9's 11.3 A surge, not the 100 mA load (`hardware-interface.md` §5) |
| Clearance | **0.5 mm** | Same margin, in the other direction |
| Pad, general (1.0 mm hole) | **2.4 mm** | 0.7 mm annular ring — hand drilling wanders |
| Pad, on 2.54 mm pitch | **1.9 mm** | Leaves a 0.64 mm gap. This is the tightest thing on the board |
| Via | **0.8 mm hole / 2.0 mm pad** | 0.5 mm wire drops through with room for solder |
| Hole, 1×N and 2×N headers | 1.0 mm | |
| Hole, Phoenix MC 1,5 | 1.1 mm | |
| Hole, TO-220 / electrolytic / fuse clip | 1.3 mm | |

### Five layout rules that only exist because there is no plating

**1. The bottom layer is the real board.** Route on the bottom (solder) side; every
through-hole part is soldered from the bottom. The top layer carries ground and the
few crossings that cannot be routed around — see rule 4 for why ground ends up on
both faces rather than only the top.

**2. A pad under a component body cannot be soldered on top.** J7's shroud, the
Phoenix bodies, the female headers the modules sit in — you cannot get an iron to
the top side of any of them. So **no top-layer track may terminate on a pad that
the component body covers**. Where the connection has to reach the top layer, put a
**via beside the pad** and a short bottom-layer track between them. Budget for
those vias now; discovering them at Stage 11 means a redesign.

**3. Nothing routes between adjacent 2.54 mm pins.** At a 1.9 mm pad the gap is
0.64 mm, and a 0.5 mm track with 0.5 mm clearance each side needs 1.5 mm. **J7's
2×20 field is a wall** — route around it, not through it.

**4. Pour GND on BOTH layers.** This is not the obvious choice and it comes
straight from counting pads. **Recounted off the placed board on 2026-09-10**, not
estimated: of the board's **217 through-holes, 135 — 62 % — are on parts whose
bodies cover them**, so they exist on the bottom only. **Thirty-six of those are
GND**: **J7's seven**, **U3's four**, **U7's four**, **CN6's two**, one each for the
six probe connectors, the four I2C branches, J13 and J14, and seven capacitor
grounds — C11 and C19's negatives plus the five 5.08 mm ceramics, whose bodies sit
over their own pads. (**C21 is no longer on this list**: bending its leads out to
5.08 mm to clear the short in its land moved its pads out from under the can, so
they can be soldered from the top.) A ground plane
on the top layer alone would need a via beside each of them, spending the entire via
budget on ground before a single signal crossed. With copper poured on both faces,
every one of those pads reaches ground directly on the bottom, and the two pours are
tied with **8–12 stitching vias** spread across the board — most densely in the 24 V
corner, where §5's surge return matters.

**5. Then keep the via count under 30, and count it as you route.** With ground
handled by rule 4, vias are spent only on signal crossings, at two per crossing.
Every one is two hand-soldered joints on a board that will sit in a sealed box for
years — they are the least reliable thing on it. **The board is 160 × 120 mm for
about forty nets**, so the answer to a crossing is almost always to route around it.
Area is the resource you have; via count is the one you do not.

### What the artwork must carry beyond the copper

- **Three registration fiducials**, 1.0 mm, **in an asymmetric L** in the margin
  outside the board outline, on **both** layers at identical coordinates. With the
  outline's bottom-left corner as the origin, and the blank cut to 180 × 140 mm:

  | | X | Y |
  |---|---|---|
  | FID1 | −7.0 mm | −7.0 mm |
  | FID2 | +167.0 mm | −7.0 mm |
  | FID3 | −7.0 mm | +127.0 mm |

  Three corners of a rectangle, never four: a sheet flipped or rotated then has no
  way to look right. They sit in the margin, so they disappear when the board is cut
  out at Stage 8.
- **A 100.0 mm scale bar** on both layers, in the bottom margin — a line from
  (30, −4) to (130, −4) with end ticks and the text `100.0 mm`. Stage 2 measures it
  with calipers on a plain-paper proof.
- **Layer identification in copper** — the word `BOT` on the bottom layer and `TOP`
  on the top. Not decoration: it is the unambiguous mirror check at Stage 4.
- **Drill dimples.** In Altium's `File » Page Setup » Advanced` (PCB Printout
  Properties) each printout has a **Holes** option. With it on, every pad prints
  with a small clear centre, and the etch leaves a copper-free dot the drill bit
  seats itself in. This is worth more than any amount of centre-punching.

> **On silkscreen:** there is none, by decision. Designators and polarity live in
> [`build-sheet-proto.md`](build-sheet-proto.md), printed 1:1 and kept with the
> board. `hardware-interface-proto.md` §7 makes checking against it a gate before
> first power.

**GATE 1** — DRC passes with the rules above; the via count is known; the
fiducials, scale bar and `TOP`/`BOT` markers are on both layers.

---

## Stage 2 — Print the artwork

Two printouts, both at **1:1**, both with **Holes** on.

| Printout | Layer | **Mirror** |
|---|---|---|
| 1 | Bottom Layer (+ bottom pour) | **OFF** |
| 2 | Top Layer (+ top pour) | **ON** |

**Where that rule comes from, so you can re-derive it instead of trusting it.**
Toner transfer puts the printed face *against* the copper, so applying the sheet
flips the image left-right. Altium plots both layers as seen from the top.

- Bottom layer plotted un-mirrored = "bottom copper, seen from the top". Flip it
  onto the bottom face → "seen from the bottom". **Correct.**
- Top layer plotted un-mirrored = "top copper, seen from the top". Flip it onto the
  top face → seen from the bottom. **Wrong** — so it must be plotted mirrored.

**The check that needs no reasoning at all:** after transfer, `BOT` must read
correctly when you look at the bottom face, and `TOP` must read correctly when you
look at the top face. If either reads backwards, that sheet was mirrored wrong.

Printer settings: **maximum toner density, toner-save off, scaling off / "Actual
size"**. Print on plain paper first and check the scale bar with calipers — a
printer set to shrink-to-fit will produce a board that is 3 % small and every
2.54 mm header will refuse to seat by the fourth pin.

**GATE 2** — the scale bar measures **100.0 ± 0.3 mm** on a plain-paper proof,
**measured centre to centre of the two end ticks**, and `TOP`/`BOT` are the right
way round on their respective sheets.

> **Centre to centre, not outside to outside.** The ticks are drawn *at* the
> 0 and 100 mm marks, so their outer edges sit half a line width further out —
> outside-to-outside reads 100.5 on a correct print, which fails this gate's
> ±0.3 mm by itself and invites rescaling artwork that was already right.
> Centre to centre is 100.0 whatever the line width is.

---

## Stage 3 — Prepare the laminate

1. Cut to **180 × 140 mm**.
2. Sand both faces — 600–1000 grit wet, or Scotch-Brite — until uniformly matt and
   bright. Copper that still has dark patches will not take toner there.
3. Wash with detergent, rinse, dry, then wipe both faces with IPA. **From here on,
   handle by the edges only.** A fingerprint is an etch resist in the wrong place.
4. **Drill the three registration holes, 1.0 mm, through the blank**, in the margin
   at the fiducial coordinates.
5. Punch or drill the same three holes in both paper sheets.

**GATE 3** — both faces matt and clean; three holes through board and both sheets;
pins drop through all of them together.

---

## Stage 4 — Transfer, both sides

Do the **bottom** side first: if something goes wrong on the second transfer, the
one you have already spent effort registering is the one you would rather keep.

1. Pin the bottom sheet to the board through the registration holes, toner side
   against the copper. Tape two edges — enough to stop it sliding, not so much that
   the pins are fighting the tape.
2. Withdraw the pins **only** once the tape holds.
3. Laminator: 3–6 passes at maximum temperature (~180 °C), same direction each
   time. Iron: medium-high, firm even pressure, ~4–5 minutes, keep it moving.
4. Let it cool to room temperature before touching the paper. Peeling warm lifts
   toner.
5. Soak in warm water 10–15 minutes; the paper pulps and comes away. Rub the
   residue off gently with a thumb, not a brush.
6. Dry, and inspect under magnification. **Touch up breaks with a permanent
   marker** — thin traces and the corners of pads are where it fails.
7. Repeat for the top side, using the same pins. **Put something smooth and soft
   under the board** so the finished bottom-side toner is not crushed against the
   laminator's roller or the bench.

**GATE 4** — `BOT` reads correctly on the bottom face, `TOP` reads correctly on the
top face, and no trace has an untouched break.

---

## Stage 5 — Registration check — **the cheap moment**

Hold the board up to a strong light. FR4 is translucent enough to see both toner
layers at once.

- Pads that exist on both layers must be **concentric within about 0.2 mm**.
- Check at all four corners, not just the middle. A rotational error looks fine in
  the centre and is worst at the edges.
- The three fiducials must line up with each other.

**If it is out, strip both sides with acetone and start Stage 3 again.** This costs
an hour. Etching a misregistered board costs the laminate, the etchant, two hours
of drilling, and it is discovered when a drill breaks out through the edge of a pad
on the far side.

**GATE 5** — both layers registered within ~0.2 mm at all four corners. **Nothing
after this point is cheap to undo.**

---

## Stage 6 — Etch

1. Warm the etchant to **40–50 °C**. Cold etchant is slow, and slow etching
   undercuts.
2. Agitate continuously. Both faces etch at once, and they do not etch at the same
   rate — the one facing up in a still tray finishes last.
3. **10–25 minutes.** Watch it; do not time it.
4. **Q1's SOT-23 footprint is the canary.** It has the finest features on the
   board. When its 0.35 mm gaps are just clear, the board is done. Leaving it
   another two minutes "to be sure" is how 0.5 mm tracks become 0.3 mm tracks.
5. Rinse thoroughly under running water. Neutralise and dispose of spent etchant
   properly — FeCl₃ does not go down a drain.

**GATE 6** — no copper bridges anywhere, and the SOT-23 gaps are clear without the
surrounding tracks being visibly thinned.

---

## Stage 7 — Strip and verify against the netlist

1. Acetone on a rag; the toner comes off in seconds.
2. **Inspect every track under magnification.** Look for hairline breaks and for
   necking where the etch ran long.
3. **Continuity-check against the netlist**, not against your memory of it. The
   high-value nets, because they are the ones whose failure is confusing rather
   than obvious:
   - `VSENS` reaching all six DQ pull-ups, all eight I2C pull-ups, U3's `VIN`, and
     all four J9–J12 `V` pins
   - each `DQ_P0`–`DQ_P5` from its J7 pin to its connector, isolated from its
     neighbours
   - `I2C_SDA` / `I2C_SCL` from J7 to the module footprint
   - **`24V_PROT` and `24V_PRE` isolated from GND and from each other**
   - the `VBAT_SENSE` divider node isolated from everything but R24/R25 and J7
4. **Repair breaks now**, with a strand of wire soldered across. It is much easier
   on a bare board than after tinning and assembly.

**GATE 7** — every net on the netlist is continuous, and no two nets are joined.
This is the last point at which the board is a *drawing* rather than an *object*.

---

## Stage 8 — Drill

**210 component holes plus vias — about 250 in all**, of which J7's 40 and the two
module headers' 28 are the ones that must be accurate. Budget two to three hours and
take breaks; this is the stage where concentration failures become mechanical
damage.

1. Back the board with scrap wood or MDF, clamped. Breakout on the far side tears
   pads off a board with no plating to hold them.
2. Work **one bit size at a time**, smallest first, so the bit changes are few:
   **0.8 mm** (vias) → **1.0 mm** (headers, J7, CN6, J13) → **1.1 mm** (Phoenix) →
   **1.3 mm** (TO-220, electrolytics, fuse clips).
3. The etched dimple seats the bit. Let it find its own centre before applying
   pressure.
4. Full speed, light feed, withdraw often on the deeper 1.3 mm holes.
5. Deburr both faces with fine paper afterwards, lightly.
6. **Now cut the board to its outline** and file the edges square. The margin has
   done its two jobs — it carried the registration holes and gave the clamp
   somewhere to bite — and the three fiducials go with it. Cutting before drilling
   would have left nothing to hold.

**GATE 8** — every hole drilled, every hole centred in its pad on **both** faces
(this is where a Stage 5 registration error finally shows), no lifted pads, board
cut to 160 × 120 mm and square.

---

## Stage 9 — Tin immediately

**Do this the same day the drilling finishes.** Freshly etched copper oxidises
within days into a surface that needs aggressive flux to wet, and the board has
~250 joints to make.

1. Wash and dry; the surface must be clean and grease-free.
2. Immerse in liquid tin per its instructions — typically a few minutes, sometimes
   warmed. The copper goes silver.
3. Rinse well, dry.

The tin does three jobs: it stops oxidation, it makes every joint easier, and it is
the only corrosion protection the copper will have until Stage 14.

**GATE 9** — uniform silver finish, no bare copper patches, no tin bridging
anything (rinse again if in doubt).

---

## Stage 10 — Vias, before any component

Every via is a piece of **0.5 mm tinned wire** soldered on both faces and snipped
flush.

Do them **all** now. A via under a component body, or beside a part already fitted,
cannot be soldered on the top face afterwards — and that is exactly the failure the
layout rule in Stage 1 was written to prevent. If one turns up here that is already
covered, the board needs a wire link on the surface instead, and it needs recording
on the build sheet.

**GATE 10** — via count matches the layout; each one continuous top-to-bottom;
none standing proud enough to foul a component that sits over it.

---

## Stage 11 — Assembly, lowest first

Order matters because a fitted tall part blocks the iron from a short one.

1. **SMD on the copper side** — D1–D6 (SOD-323) and Q1 (SOT-23). Flat against the
   board, nothing else in the way yet.
2. Axial resistors, lying flat.
3. Diodes — D9 (P6KE33A), D10, D11, D12. **All four are polarised.**
4. Ceramic capacitors.
5. Q4 (TO-92), and the 2.54 mm headers — J7, CN6, J13, and the female headers for
   the two modules.
6. Phoenix terminals J1–J6, J9–J12, J14.
7. Electrolytics C11, C19, C21. **Polarised.**
8. Fuse clips and F1.
9. Q2 and Q3 (TO-220), Q3 with its heatsink. **Their tabs are drains and live** —
   see `hardware-interface-proto.md` §6.
10. **The modules last**, into their headers, and only after the pre-fit audit in
    [`build-sheet-proto.md`](build-sheet-proto.md) has been done on the TCA9548A
    board and after §7's checks on `24V_PRE`.

**GATE 11** — every polarised part checked against the build sheet **before** power
is connected. There is no silkscreen to catch this later.

---

## Stage 12 — Test, before coating

Run `hardware-interface.md` §7 in full, with the additions in
`hardware-interface-proto.md` §7. Do not coat a board that has not passed.

The three that are specific to this process:

- **`24V_PRE` under 28 V with the bank at its highest.** The whole reason the
  pre-regulator exists.
- **Whole-board standby current** at `24V_PROT`, gate closed. **Expect ~0.6 mA** —
  0.5 mA of module (its datasheet) plus 0.1 mA of R41 bias.
- **Q3 barely warm** during an SCD41 burst. Hot means the current limiter is
  engaging.

**GATE 12** — §7 passes end to end. **Coating an untested board means removing
coating to fix it.**

---

## Stage 13 — Clean

Wash the flux off with IPA and a brush, and dry completely — warm air, or an hour
somewhere dry. Flux residue under conformal coating traps moisture against the
copper, which is the opposite of the point.

---

## Stage 14 — Conformal coat

**Mask first, and mask more than feels necessary:**

- J1–J6, J9–J12, J14 — the Phoenix contacts
- CN6 and J7 — the pins the Nucleo cables land on
- J13 — the debug header
- F1's fuse clips
- The female headers the two modules plug into
- Q3's heatsink, if it is the clip-on kind you may want to remove

Two thin coats, the second after the first is touch-dry. Thin coats; a thick one
traps solvent and stays soft.

**GATE 14** — no coating on any contact surface; the board still seats its
connectors; the fuse still comes out of its clips.

---

## Working through this together

The stages that are worth stopping on, because they are decision points rather than
manual work:

| Stage | What we do together |
|---|---|
| **1** | Set the design rules in Altium, place the fiducials, scale bar and layer markers, count the vias, find the pads that need a via beside them |
| **2** | Confirm the exact print dialog in this Altium version and get **Holes** and **Mirror** right — the two settings that are silent when wrong |
| **5** | Read the registration result and decide continue-or-restart |
| **7** | Work the netlist check as a list, net by net |
| **12** | Run §7 with the proto additions |

Everything between those is bench work and does not need me.

---

Thai mirror: [`pcb-home-etch.th.md`](pcb-home-etch.th.md) — that is the copy to
print for the bench.
