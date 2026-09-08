# water-temp-node — `STM32WL_Proto`, the self-etched board

**This board replaces `STM32WL_FE` as the deployed node.** It is not a bench rig and
not a stepping stone. It hangs in the same IP65 box outside the mushroom house, on
the same 24 V solar bank, and every survival requirement in §5 of
[`hardware-interface.md`](hardware-interface.md) applies to it in full.

What makes it different is how it is built: **etched at home, double-sided, toner
transfer, hand-drilled, hand-soldered**, with modules standing in for the two parts
that cannot be built that way. Board area is free; fine pitch is not.

---

## 0. What this document is, and is not

**It is a delta.** [`hardware-interface.md`](hardware-interface.md) remains the
specification. This file does not copy it, restate it, or summarise it, because a
second copy of a table is a table that goes stale — which is the lesson §1 of
[`pcb-altium.md`](pcb-altium.md) recorded after three spec defects were found by
comparing documents rather than by any tool.

| Section of the shared spec | Applies to this board |
|---|---|
| §1 — the joint, which bus and why | **unchanged**, read it there |
| §2 — signals crossing the connector, CN10 pin map, CN6 power tap, keying | **unchanged**, read it there |
| §3 — sensor front-end: probes, pull-ups, mux, SCD41, rail gate, settle time | **unchanged** except U3's package, §3 below |
| §4a — bill of materials | **overridden** by §2 below: every package changes |
| §4b — parts out at the sensors | **unchanged**, read it there |
| §5 — power, 24 V solar | **partly overridden** by §4 below: the regulator and everything new around it. F1, Q2, D9, C11 and the `VBAT_SENSE` divider keep their §5 reasoning |
| §6 — cables and enclosures | **unchanged**, read it there |
| §7 — bring-up order | **extended** by §7 below, not replaced |

Where this file and `hardware-interface.md` disagree about anything not listed as
overridden above, that one is right and this one is a bug.

The build process — registration, mirroring, drilling, tinning, coating — is not
here either. It is in [`pcb-home-etch.md`](pcb-home-etch.md), and the per-board
assembly and polarity checklist is [`build-sheet-proto.md`](build-sheet-proto.md).

---

## 1. What changed, and what did not

**Nothing in the schematic's *connectivity* changed except the power section.** The
nets, the signal names, the connector contract, the pin map and the firmware are
all identical to `STM32WL_FE`. That is deliberate and it is what makes this board
cheap to design: it is the same circuit in different packages, plus one new block.

Three consequences worth stating up front, because each one is a thing that does
*not* need re-deriving:

- **The Nucleo interface is untouched.** Still a 2×20 IDC box header at J7 to
  morpho CN10, still a separate 1×8 lead to CN6, still the same two keying tricks
  (clip CN10-6, clip CN6-1). §2's whole argument survives, including why power
  stays off the ribbon.
- **The firmware needs no change at all.** Same pin map, same `SENS_GATE` on PA8,
  same mux at `0x70` (`node_config.h:164`), same `VBAT_SENSE` divider on PB3. The
  one thing to re-verify at bring-up is `DS_POWER_SETTLE_MS` / `I2C_POWER_SETTLE_MS`
  (both 10 ms) — see §3, because a breakout module brings its own capacitance to
  the `VSENS` rail.
- **§3's electrical values are all still correct.** 4.7 kΩ on the DQ lines,
  2.2 kΩ downstream and 4.7 kΩ upstream on I2C, ≤10 µF of `VSENS` bulk, the
  100 Ω–1 kΩ gate resistor. None of them depend on package.

What did change:

| | `STM32WL_FE` | `STM32WL_Proto` |
|---|---|---|
| Fabrication | 2-layer fab house, soldermask, silkscreen, plated holes | **self-etched 2-layer**, no mask, no silkscreen, wire vias |
| Passives | 0805 | through-hole axial / radial |
| Regulator | Traco TSR 1-2433, SIP-3 | **DFRobot DFR0570 module + linear pre-regulator** |
| U3 mux | TCA9548APWR, TSSOP-24 | **TCA9548A breakout module** |
| Q2 | DMP6023LE-13, SOT-223 | IRF9540N, TO-220 |
| D9 | SMBJ33A, DO-214AA | P6KE33A, DO-15 |
| D10 | BZX84C12, SOT-23 | 1N4742A, DO-41 |
| D1–D6, Q1 | SOD-323 / SOT-23 | **unchanged — still SMD**, §5 below |
| New refs | — | Q3, Q4, D11, D12, R41, R42, C22, net `24V_PRE` |
| Board | as laid out | **~160 × 120 mm**, connectors on three edges |

---

## 2. §4a-proto — the parts list

Values come from §4a and §3 of the shared spec and are not re-derived here. Only
the **package** column is this document's own, plus the seven new references.

### Unchanged in value, changed in package

| Ref | Value | Package on this board |
|---|---|---|
| R1 | 100 kΩ | axial ¼ W |
| R2 | 100 Ω | axial ¼ W — Q1 gate series resistor |
| R3–R8 | 4.7 kΩ | axial ¼ W — six DQ pull-ups, on `VSENS` |
| R9–R14 | 100 Ω | axial ¼ W — DQ series |
| R15, R16 | 4.7 kΩ | axial ¼ W — upstream I2C, on `VSENS` |
| R17–R22, R39, R40 | **2.2 kΩ** | axial ¼ W — one pair per mux channel, on `VSENS` |
| R23 | 10 kΩ | axial ¼ W — U3 `RESET` pull-up, **not optional** |
| R24 | 300 kΩ | axial ¼ W — `VBAT_SENSE` divider top |
| R25 | 30 kΩ | axial ¼ W — divider bottom |
| R38 | 470 kΩ | axial ¼ W — Q2 gate pull-down |
| C1 | 1–10 µF | ceramic or radial — `VSENS` bulk, **see the ≤10 µF ceiling in §3** |
| C2, C8, C10 | 100 nF | ceramic, 5 mm pitch |
| C11 | 100 µF **≥63 V** | radial electrolytic, on `24V_PROT` — 63 V because D9 clamps to 53.3 V |
| C21 | 22 µF 16 V | radial — optional, the module has its own output capacitor |
| F1 | T2A, 5×20 mm | cartridge + PTF-78 holder — **sized by I²t, see §5** |
| J1–J6 | — | Phoenix MC 1,5/3-G-3,5 |
| J9–J12 | — | Phoenix MC 1,5/4-G-3,5 |
| J13 | — | 1×3 header, 2.54 mm |
| J14 | — | Phoenix MC 1,5/2-G-3,5 |
| J7 | — | 2×20 boxed IDC header, 2.54 mm |
| CN6 | — | 1×8 header, 2.54 mm |

### Changed part

| Ref | `STM32WL_FE` | Here | Why it is a valid substitution |
|---|---|---|---|
| Q2 | DMP6023LE-13, −60 V | **IRF9540N**, TO-220, −100 V | §5 requires ≥60 V; 100 V clears it. Rds(on) 0.117 Ω at V_GS −10 V, and D10 clamps the gate at −12 V, so it is fully enhanced. At 100 mA the drop is 12 mV — 0.05 % of a 24 V reading, still far under the divider's own tolerance |
| D9 | SMBJ33A, DO-214AA | **P6KE33A**, DO-15 | The through-hole member of the same family: 600 W, unidirectional, 33 V standoff, **53.3 V clamp** — the number §5's "≥63 V" rule on C11/C19 is derived from is unchanged |
| D10 | BZX84C12, SOT-23 | **1N4742A**, DO-41 | 12 V 1 W Zener, same function. **DO-41 has two leads, so the SOT-23 pin trap of §1 cannot recur** — but the band is still the cathode and it still goes to the source |
| U3 | TCA9548APWR, TSSOP-24 | **TCA9548A breakout module** | §3 below |
| U7 | Traco TSR 1-2433 | **DFRobot DFR0570 + pre-regulator** | §4 below |

### Kept as SMD — see §5 for why

| Ref | Part | Package |
|---|---|---|
| D1–D6 | Bourns CDSOD323-T05LC | SOD-323, two leads |
| Q1 | AO3401A | SOT-23 |

### New on this board

| Ref | Part | Package | Function |
|---|---|---|---|
| Q3 | IRF740 (400 V) or IRF640N (200 V) | TO-220 **+ small heatsink** | pre-regulator pass element |
| D11 | 1N4750A, 27 V 1 W | DO-41 | pre-regulator gate reference |
| D12 | 1N4744A, 15 V 1 W | DO-41 | Q3 gate–source clamp |
| R41 | 22 kΩ ¼ W | axial | D11 bias |
| R42 | **4.7 Ω ½ W** | axial | current-limit sense |
| Q4 | BC547 | TO-92 | current-limit sense transistor |
| C22 | 100 nF | ceramic | Q3 gate soft-start |
| C19 | 100 µF 35 V | radial electrolytic | **moved** — now the module's local input bulk on `24V_PRE`, not on `24V_PROT`. §4 |

---

## 3. U3 — the mux is a module now

The TCA9548A is TSSOP-24 on a 0.65 mm pitch. This process resolves 0.3 mm at best
and has no soldermask to stop bridges, so the bare chip is not buildable here. The
breakout module is the right answer and it is also the one the "use modules"
principle was about.

**What it costs is certainty about what is on it.** Breakout boards carry their own
pull-ups and their own decoupling, and this design has fourteen carefully-sized
pull-ups and a bulk ceiling. Every one of those interactions is survivable; one of
them is not automatic.

| What is likely on the module | Effect here | Verdict |
|---|---|---|
| ~10 kΩ upstream SDA/SCL pull-ups | parallel with R15/R16 4.7 kΩ → 3.2 kΩ over 30 mm of trace | **fine** |
| 10 kΩ downstream pull-ups, if fitted | parallel with 2.2 kΩ → 1.8 kΩ | **fine** — §3 puts the stiffest usable pull-up at ~1.5 kΩ, set by the SHT45 having to sink 3 mA at 0.4 V |
| Pull-up on `RESET` | parallel with R23 10 kΩ → 5 kΩ | **fine** |
| Decoupling capacitor | adds to the `VSENS` rail | **check it** — see below |

**Two things are not optional.**

**1. The module's `VIN` goes to `VSENS`, never to permanent 3V3.** This is §2's rule
— *the pull-ups sit on `VSENS`, never on permanent 3V3* — and a module makes it
easy to break, because a breakout labelled `VIN` invites a connection to the
nearest rail. Get it wrong and the module's own pull-ups hold the bus high with the
gate closed, the mux draws standby current continuously, and `SENS_GATE` stops
being the complete fault-recovery mechanism §3 says it is. Tie A0/A1/A2 to GND on
our board for address `0x70`.

**2. If the module's decoupling is over 1 µF, take it out of C1's budget.** §3 caps
total `VSENS` bulk at **10 µF**, because `DS_POWER_SETTLE_MS` is 10 ms and because
inrush runs through Q1. A breakout with a 10 µF tantalum on it spends that entire
budget by itself. Measure it before fitting and reduce C1 to match; if the total
still lands over 10 µF, the settle constants are the thing that has to be
re-verified at bring-up, not the thing to leave alone.

Both checks, with the others, are on the pre-fit audit in
[`build-sheet-proto.md`](build-sheet-proto.md). They take a multimeter and two
minutes, and they are much cheaper before the module is soldered down.

---

## 4. §5-proto — power

### 4.1 The 28 V objection, and how it is answered

§5 rejects this module by name: *"Do not substitute a 28 V module. DFRobot
DFR0570/0571, MP1584 and LM2596 boards are all rated 28 V maximum input. A 24 V
lead-acid bank sits at 28.8 V on absorb; an 8S LiFePO4 bank reaches 29.2 V full.
That is not a fault case, it is every sunny afternoon."*

**That objection is correct and it is not withdrawn.** The DFR0570 is
5.5–28 V in, 3.3 V fixed, 3 A. What changes is that on this board the module never
sees the bank. A linear pre-regulator sits between `24V_PROT` and the module and
holds its input at roughly 21–23 V regardless of what the bank does, including
during a TVS event.

The module does pass §5's other regulator test — **fixed output, not adjustable**.
3.3 V is a factory value with no trim pot, so the number that protects CN6-4 and
the STM32WL from the 3.6 V ceiling is not something a screwdriver can move.

### 4.2 The pre-regulator

```
   24V_PROT ──┬─────────────┬──── D ┐
              │             │       │ Q3  IRF740
           [R41 22k]        │       │
              │             │       └ S ──┬──[R42 4.7R]──┬───── 24V_PRE
              ├──────┬──────┘             │              │
              │      │                    │   Q4 BC547   │
          [D11 27V] [C22 100n]            └───b   c──────┤ (c → Q3 gate)
              │      │                        │e         │
             GND    GND                       └──────────┤
                                                         │
              [D12 15V] gate ── source                [C19 100µ/35V]
                                                         │
                                                        GND
```

Q3 is a source follower. D11 pins its gate, so the source — and therefore the
module's input — can never rise more than a gate-source drop above the Zener,
whatever arrives at `24V_PROT`.

| `24V_PROT` | D11 conducts at | `24V_PRE` | Module input rating |
|---|---|---|---|
| 18 V (bank low) | off — gate follows input | ~14 V | ✅ min 5.5 V |
| 25.5 V (float) | ~23 V, ~0.1 mA | ~19 V | ✅ |
| **29.2 V (8S LiFePO4 full)** | ~25 V, ~0.19 mA | **~21 V** | ✅ well under 28 V |
| **53.3 V (D9 clamping a surge)** | 26.5 V, ~1.2 mA | **~23 V** | ✅ the transient never reaches the module |

**D11 is a limiter, not a precision reference, and that is the whole trick.** At
these bias currents a 1 W Zener sits well below its nominal knee and its voltage
drifts with input — which does not matter, because the only requirement is that the
output stay between 5.5 V and 28 V, and it does across a 3:1 input range.

**Component reasoning:**

- **R41 = 22 kΩ.** At 1 MΩ the bias would be ~2 µA, far below any part of the
  Zener's characteristic, and the clamp would not be where it is drawn. At 2.2 kΩ
  the bias would be ~1 mA, which is 24 mW burnt continuously — 0.58 Wh/day, and
  §5's energy table is only 0.8 Wh/day in total. 22 kΩ costs **~0.1 mA, about
  0.06 Wh/day**, and still puts the Zener somewhere useful.
- **C22 = 100 nF** gives τ = 2.2 ms of gate ramp. That is soft-start, and it does a
  second job: it turns hot-plug inrush into a ramp instead of a step.
- **D12 = 15 V, gate to source.** Without it, a short on `24V_PRE` leaves the gate
  at 27 V and the source at 0, which is 27 V across a ±20 V gate oxide.
- **R42 = 4.7 Ω with Q4** limits at V_be/R42. The worst case is not the nominal
  0.65 V but a **hot** transistor: V_be falls about 2 mV/°C, so the limit falls with
  it. Against a load that reaches **59 mA** when the bank is low (the DFR0570 is a
  constant-power load — 0.83 W in for the SCD41's 205 mA burst — so its input
  current *rises* as its input voltage falls):

  | R42 | limit at 0.70 V | at 0.65 V | at 0.55 V (hot) | margin over 59 mA | Q3 into a dead short |
  |---|---|---|---|---|---|
  | 3.3 Ω | 212 mA | 197 mA | 167 mA | 2.8× | 4.7 W |
  | **4.7 Ω** | **149 mA** | **138 mA** | **117 mA** | **2.0×** | **3.3 W** |
  | 6.8 Ω | 103 mA | 96 mA | 81 mA | 1.4× | 2.3 W |

  6.8 Ω runs out of margin before it runs out of heat. 4.7 Ω is the compromise, and
  it is why **Q3 needs a small clip-on heatsink** — 3.3 W in a bare TO-220 is about
  200 °C of rise and the part does not survive a sustained short without one.

- **C19 moves to `24V_PRE`.** On `STM32WL_FE` it was the module's `C_IN` on
  `24V_PROT`. Here the module's input node *is* `24V_PRE`, so C19 follows it —
  which also gives the current limiter a smooth DC load instead of the buck's
  500 kHz input pulses, and lets its voltage rating drop from 63 V to 35 V because
  that node is clamped. **C11 stays on `24V_PROT` at ≥63 V**, unchanged: its job is
  damping the input cable's resonance and sitting inside D9's clamp, and §5's
  reasoning for it is untouched.

### 4.3 Three costs, stated plainly

**1. Low-bank cutoff moves up by about 5 V.** The TSR ran down to 4.75 V in. The
pre-regulator drops a gate-source volt-drop, so the module drops out with the bank
near **9.5 V** instead of near 5 V. §5 says the node reports `VBAT_SENSE` down to
6.5 V; on this board it reports down to about 9.5 V. A 24 V bank at 9.5 V is
already destroyed, so this is a reporting loss, not an operating one — but it is a
loss and it belongs in the field notes.

**2. The idle budget is no longer known.** §5's table is dominated by one line —
*72 % of the budget is a regulator doing nothing*, the TSR's **1 mA typ** no-load
current. **The DFR0570's no-load current is not published**, and a 3 A module is
not likely to beat a 1 A one at idle. Add R41's ~0.1 mA and the honest position is
that this board's standby draw is unmeasured. It is still almost certainly a small
fraction of 80 Wh/day — but §7 step 1 now measures it rather than assuming it.

**3. A sustained short on `24V_PRE` is survivable, not free.** The limiter holds it
to ~130 mA and the heatsink carries the 3.3 W, indefinitely and without F1 opening
(F1 is a 2 A part; 130 mA will never blow it). The board does not damage itself —
but nothing announces the fault either, and the node simply looks dead. On a board
with no soldermask, a solder bridge across the module's `VIN`/`GND` pins is a
realistic build error, which is why the limiter is here at all and why
[`build-sheet-proto.md`](build-sheet-proto.md) checks that node before first power.

### 4.4 What is unchanged from §5

F1 → Q2 → D9 → C11 keeps every word of its reasoning: the fuse sized by I²t against
the body-diode inrush, Q2 wired **drain to supply, source to load** with R38 and D10
as a mandatory pair, D9 unidirectional and downstream of Q2, D9's anode and C11's
negative given their own wide copper straight back to J14's ground pin, and R24/R25
last and farthest from the switching node. Read them there. The only thing added to
that chain is Q3's block, which hangs between C11 and the module.

---

## 5. The two parts that stay SMD

Everything else on the board is through-hole. These two are not, and the reason is
that no through-hole part meets their specification.

**D1–D6 — the 1-Wire TVS.** §3's requirements are `V_RWM` **≥ 3.6 V** (because
`VSENS` reaches 3.6 V on a fresh cell), capacitance **≤ 50 pF**, and leakage
budgeted **in µA, never mA**. Through-hole TVS parts — P6KE, 1.5KE — are the power
class: hundreds to thousands of pF and leakage that, against a 4.7 kΩ pull-up,
stops the DQ line ever reaching a valid high on a hot day. §3 names that failure
explicitly and calls it *"a node which passes on the bench and dies in the sun."*
The Bourns CDSOD323-T05LC stays.

**Q1 — the `VSENS` gate.** Its gate is driven by `SENS_GATE` from a 3.3 V rail, so
it needs full enhancement at V_GS = −3.3 V. Common TO-220 P-channel parts specify
V_GS(th) at −2 to −4 V and are barely conducting there. The AO3401A stays.

**Neither is hard to solder on this board, and the reason is worth knowing:**
SOD-323 is two leads about 2.5 mm apart, which is *coarser* than the 2.54 mm
header pitch elsewhere on the board. Soldermask exists to stop bridges between
adjacent pins, and a two-lead part has no adjacent pins to bridge. Both are
soldered directly to pads on the copper side.

**SOT-23 is the one real exception.** Q1's 0.95 mm pitch needs ~0.6 mm pads with
~0.35 mm gaps, which is below the 0.5 mm design rule and into the margin of what
toner transfer resolves. It is one footprint on the whole board; etch it carefully
and inspect it under magnification before soldering.

---

## 6. Board outline

**~160 × 120 mm, connectors on three edges.** Board area is free here and the
constraint is edge length, not area: the through-hole connectors need about 256 mm
of board edge between them, which does not fit on one side of any sensible
rectangle.

```
                          160 mm
      ┌──────────────────────────────────────────┐
   J7 │ [2×20 IDC]        [TCA9548A module]      │ J9   ┐
      │                                          │ J10  │ I2C branches
  CN6 │ [1×8]             [DFR0570]              │ J11  │ ≤5 m each
      │                                          │ J12  ┘   120 mm
      │ [Q3 ▮hs] [D11][D12][R41][R42][Q4][C22]   │
      │ [F1] [Q2 ▮] [D9] [C11] [C19]             │
      └──────────────────────────────────────────┘
        J1   J2   J3   J4   J5   J6        J14
        └──── probes P0–P5, 5–10 m ────┘   24 V in
```

Placement rules that come from the shared spec and are not negotiable here:

- **The 24 V section stays in one corner, at the J14 edge**, with D9's anode and
  C11's negative on their own wide copper back to J14's ground pin. §5's ordering
  rule — D9 nearest the door, C11 next, module input bulk within 5 mm of the module
  pins, R24/R25 last — applies to the physical sequence along the trace.
- **`24V_PROT` and the ground return are 3 mm copper**, sized by D9's 11.3 A surge,
  not by the 100 mA load.
- **No traces under the module footprints**, and none under Q3's heatsink.
- **Probes P0–P5 keep their numbering along the edge**, matching `DS_PROBE_BUSES`
  and the dashboard metric names, because §6 makes the connector labelling part of
  the design.

**Two mechanical warnings specific to TO-220 here.** The tab of a TO-220 is the
**drain**: Q2's tab sits at `24V_PROT` and Q3's tab sits at `24V_PRE`. Neither may
touch the enclosure, a mounting screw, or each other — use isolated mounting or
keep them clear. And Q3's heatsink is at `24V_PRE` unless it is isolated.

---

## 7. Additions to §7, bring-up

§7's order stands. These are inserted, not substituted.

**Before step 1 — with no power connected at all:**

1. **Polarity check against the build sheet.** This board has no silkscreen. C11,
   C19, D9, D10, D11, D12 and both electrolytics are polarised, and §5 names
   reversed C11, reversed C19 and reversed D9 as three of the ways this node gets
   destroyed. Check every one against
   [`build-sheet-proto.md`](build-sheet-proto.md) before the first volt.
2. **Continuity on `24V_PRE` to ground.** A solder bridge at the module's input
   pins is invisible on unmasked copper and is exactly what the current limiter
   exists to survive — but surviving it is not the same as finding it.
3. **Buzz out both module footprints.** §5's second trap for the TSR applies with
   more force to a module on a hand-drilled board: a footprint mirrored or rotated
   puts 24 V on an output pin, and the Nucleo is downstream.

**At step 1, in place of "measure the module's no-load current":**

4. **Measure the whole board's standby current** at `24V_PROT`, gate closed, radio
   idle. §5's 1 mA figure was the TSR's; the DFR0570's is unpublished. Record what
   you actually get — it is the number the energy budget rests on, and it is now
   the only unmeasured line in that table.

**After the rail is up:**

5. **`24V_PRE` with the bank at its highest.** Read it with the bank on absorb, or
   force the input to 29.2 V from a supply. **It must be under 28 V** — that is the
   entire justification for the pre-regulator and it takes one meter reading.
6. **Q3 case temperature under the SCD41 burst.** Expect barely warm. Hot means the
   current limiter is engaging, which means something downstream is drawing far
   more than 59 mA.
7. **`VSENS` rise time**, if the mux module's onboard capacitance came out over
   1 µF: confirm the rail is up inside `DS_POWER_SETTLE_MS`, and that the first
   1-Wire reset sees a valid bus.

---

## Open items

- **Where the vendor libraries live.** `AO3401A` and `CDSOD323-T05LC` are shared
  with `STM32WL_FE`, which currently holds them in its own `Lib/`. See §2.1 of
  [`pcb-altium.md`](pcb-altium.md) — decide before this board's `.PrjPcb` exists.
- **Q3 part number** is IRF740 or IRF640N pending local stock; anything N-channel
  with V_DS ≥ 100 V and V_GS(th) under 4 V works, and the 400 V rating of the
  IRF740 is incidental, not a requirement.
- **The DFR0570's no-load current** is unpublished and becomes known at §7 step 4.
- **`hardware-interface-proto.th.md`** — the Thai mirror is not written yet.
