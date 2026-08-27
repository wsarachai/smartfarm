# water-temp-node — hardware interface spec

The real (non-breadboard) build is **two PCBs joined by a connector**:

| Board | What it is | Carries |
|---|---|---|
| **Brain** | NUCLEO-WL55JC1 (off-the-shelf) | STM32WL55JC, radio, RF switch, TCXO, antenna, ST-LINK |
| **Front-end** | Custom PCB, this spec | DS18B20 probe connectors, A0341 P-MOSFET rail gate, pull-ups, line protection, battery input |

The front-end mounts **above** the Nucleo on its ST morpho headers. This document
specifies that joint: the connector, the signals crossing it, and the front-end
circuit behind them.

Everything here is a contract the **firmware already assumes**. Where a value is
forced by code, the code is cited — change one and you change the other.

- Firmware pin map: [`include/node_config.h`](../include/node_config.h)
- Gate + sleep handling: [`src/main.cpp`](../src/main.cpp)
- Cable-length guidance: [README → *Long DS18B20 runs*](../README.md)

---

## 1. The stack

```
        ┌─────────────────────────────────────────────┐
        │  FRONT-END PCB                              │
        │  probe J1/J2 · gate Q1 · pull-ups · TVS     │   <- this spec
        │  battery J3                                 │
        │   [2x19 socket]            [2x19 socket]    │
        └──────┬──────────────────────────┬───────────┘
               │  CN7                CN10 │              2.54 mm, 38 pos each
        ┌──────┴──────────────────────────┴───────────┐
        │  NUCLEO-WL55JC1     [antenna keep-out]      │
        │  (ST-LINK section snapped off — see §5)     │
        └─────────────────────────────────────────────┘
```

- **Mating:** the Nucleo's morpho **CN7/CN10** are 2×19 male pin headers on
  2.54 mm pitch. The front-end carries two matching **2×19 female socket strips**
  on its underside.
- **Mechanical:** sockets at both long edges support the board at both edges — no
  standoff strictly required, but fit **M3 nylon standoffs** at the Nucleo's
  mounting holes anyway. A board held only by header friction will fret its
  contacts loose under vibration and thermal cycling.
- **Stack height:** the socket strip must clear the Nucleo's tallest parts (USB
  connector, antenna connector). Use tall/stacking sockets or add spacers, and
  confirm against your actual board before ordering.
- **Antenna keep-out:** cut the front-end board away over the Nucleo's RF section
  and antenna. No copper, no battery, no probe cable routed over it.

### Keying — do this, it is the one irreversible mistake

Two identical 2×19 headers, symmetric about the board centre, will mate **rotated
180°** just as happily as the right way round. That puts `VBAT` onto a signal pin.

Pick at least one:

1. **Depopulate one position as a key** — clip a single unused pin on the Nucleo
   header and plug the matching socket hole. Cheap and absolute.
2. **Asymmetric outline** — extend the front-end board over one end only, so it
   fouls the USB/antenna if reversed.
3. **Silkscreen pin-1 triangles** on both boards, plus a printed arrow. Necessary
   but *not* sufficient on its own — never rely on this alone.

---

## 2. Signals crossing the connector

Six signals. Freeze this table; it is the interface. Everything else on the
morpho headers is **left unconnected** on the front-end — do not casually route
extra pins "in case", each one is a new way to fight the radio or the debugger.

| Signal | WL55 pin | Direction | Electrical | Forced by |
|---|---|---|---|---|
| `VBAT` | — | in to brain | 3.0–3.6 V, ≤150 mA peak during TX | `battery.cpp` — battery **is** VDDA |
| `GND` | — | — | use **≥2 GND pins**, one per header | — |
| `SENS_GATE` | **PA8** | out of brain, push-pull | active **LOW** = rail on; Hi-Z = off via 100 k | `node_config.h:39` |
| `DQ_HOT` | **PA10** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h:25` |
| `DQ_COLD` | **PA9** | bidir, open-drain | 1-Wire; parked **analog** in sleep | `node_config.h:29` |
| `VSENS` | — | front-end internal | gated 3V3 to probes **and** pull-ups | `main.cpp:64` |

`VSENS` is generated **on the front-end** and never crosses back to the Nucleo.
The only power crossing the joint is `VBAT` and ground.

Optional, bring-up only, not part of the contract: SWD (PA13/PA14) and the
LPUART1 VCP (PA2/PA3).

### Two rules the front-end must honor

**1. The pull-ups sit on `VSENS`, never on permanent 3V3.** On the always-on rail,
each gated-off probe becomes a leakage path through its DQ clamp diode and the
15-minute duty cycle stops meaning anything. `main.cpp:68` parks both DQ pins as
analog for the same reason — the board has to cooperate.

**2. `SENS_GATE` is active-low with a 100 k gate→source pull-up.** That resistor is
not optional: it holds the sensor rail **off** while PA8 is Hi-Z — during reset,
during BOOT0, and before firmware runs.

### Pin choice

PA8/PA9/PA10 deliberately avoid PC3/PC4/PC5 (RF switch), PA13/PA14 (SWD),
PA2/PA3 (VCP) and PB0 (TCXO) — see the header comment at `node_config.h:5`.

**Confirm the exact CN7/CN10 row for each of these against UM2592's morpho pinout
table before committing copper.** The signal-to-MCU-pin mapping above is certain;
the mapping to physical header positions is the detail that costs a board spin.

---

## 3. Sensor front-end circuit

One point-to-point line per probe — no shared bus, which is what makes the
driver's SKIP ROM legal. Both channels identical:

```
                    ┌──────── VSENS (switched) ────────┬─────────────┐
                    │                                  │             │
                  [2.2k]                             [100nF]      to probe
                    │                                  │          VDD pin
   PA10 ──[100R]────┼──────────────────────────────────┼────────── DQ
                    │                                  │
                   TVS                                GND ───────── GND
              (low-C, bidir)                      (twisted with DQ)
                    │
                   GND
```

The rail gate, upstream of both channels:

```
   VBAT ──────┬──────────────── S │ Q1 (P-MOSFET) │ D ──┬──── VSENS
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
| Pull-up | **2.2 kΩ** | For the 10–20 m run. 4.7 kΩ is the short-bus value; 2.2 kΩ buys back rise time against cable capacitance. Keep ≥1.5 kΩ so the DS18B20 can still pull a valid low |
| Series R | **100 Ω** | Reflection damping, and the sacrificial element ahead of the TVS |
| TVS | bidirectional, **<50 pF** | 20 m of wet cable is an antenna. Capacitance is the spec that matters — high-C protection rounds off the bit slots |
| Cap at probe | **100 nF** | At the **far** end across the probe's VDD/GND, not on the PCB |
| Bulk on `VSENS` | **1–10 µF** | Bounded by the settle time below |

### The settle-time constraint

`DS_POWER_SETTLE_MS` is **10 ms** (`node_config.h:43`). The switched rail must be
fully up inside that window. Keep `VSENS` bulk ≤10 µF and put **100 Ω–1 kΩ in
series with the gate** to tame inrush — that lands around a millisecond.

> Oversize that cap and the failure mode is intermittent CRC errors on the first
> read after each wake. Miserable to diagnose in the field.

### On the P-FET

At 3.3 V of gate drive an AO3401-class part gives milliohms against a ~3 mA load —
wildly overkill, which is fine. The parameter that actually matters here is
**drain-source leakage (I_DSS)**, because at a 15-minute duty cycle the part is off
99.9% of the time. Check it on your specific datasheet: at ~1 µA it is the same
order as the STM32's Stop2 current, i.e. the power gate becomes a real fraction of
the battery budget. A low-leakage P-FET or a load-switch IC with a spec'd sub-100 nA
off current is the upgrade.

---

## 4. Bill of materials — front-end PCB

| Ref | Part | Value / spec | Notes |
|---|---|---|---|
| J4, J5 | Socket strip 2×19, 2.54 mm | female, underside | Mates CN7/CN10. **Key one position** (§1) |
| Q1 | P-MOSFET SOT-23 | A0341 / AO3401-class, Vgs(th) ≤ −1.5 V | High-side gate; check I_DSS |
| R1 | Resistor 0805 | 100 kΩ | Gate→source pull-up — **holds the rail off at reset** |
| R2 | Resistor 0805 | 100 Ω–1 kΩ | Gate series, inrush limit |
| R3, R4 | Resistor 0805 | 2.2 kΩ | DQ pull-ups, **on `VSENS`** |
| R5, R6 | Resistor 0805 | 100 Ω | DQ series |
| C1 | Ceramic | 1–10 µF | `VSENS` bulk (see settle time) |
| C2 | Ceramic | 100 nF | `VSENS` decoupling |
| D1, D2 | TVS bidirectional | <50 pF, ~5 V standoff | One per DQ line |
| J1, J2 | Pluggable screw terminal, 3-pin | Phoenix MC 1,5/3-ST-3,5 or clone | One per probe |
| J3 | Battery connector | keyed, 2-pin | LiFePO4 or 2× lithium AA |
| — | M3 nylon standoffs + screws | ×4 | Nucleo mounting holes |
| TP1–5 | Test pads | — | `VSENS`, both DQ, `SENS_GATE`, `VBAT` |

At the probe end (not on the PCB): **100 nF** across each probe's VDD/GND.

---

## 5. Power — two traps specific to a Nucleo in the field

**The ST-LINK will eat the battery.** A Nucleo's debug section draws milliamps,
fatal for cells. Either feed 3V3 directly to the target (on Nucleo-64 boards this
leaves ST-LINK unpowered, so no debugging — verify the jumper/solder-bridge
configuration in UM2592 for your board revision), or **snap off the ST-LINK
section**; these boards are scored for it. Then measure: if Stop2 current is not
single-digit µA, something on the board is still alive.

**Battery chemistry is constrained by the DS18B20, not the MCU.** The STM32WL runs
to 1.8 V but the **DS18B20 needs ≥3.0 V**, so 2×AA alkaline (sagging to ~2.0 V)
kills the sensors long before the radio quits. Use **LiFePO4 (3.0–3.6 V)** or
**2× lithium AA (L91)** — the rail `battery.cpp` already assumes.

> **Coupling to firmware:** `battery_read_mv()` reads VDDA via VREFINT. Add a
> regulator between battery and MCU and it starts reporting the regulator output
> instead of the battery — you would then need a divider plus an ADC pin added to
> the §2 contract.

---

## 6. Probe cable and enclosure

- **Probes:** 3-pin pluggable screw terminals. Field re-termination with cold hands
  is the design case; JST crimps will not survive it. Silkscreen `V / DQ / G` and
  mark **HOT** vs **COLD** — the distinction is purely which connector the probe
  lands in.
- **Cable:** one twisted pair = **DQ + GND**, VDD on a separate conductor. Cat5 is
  ideal. Biggest single reliability factor at 20 m.
- **Enclosure:** IP65, cable glands sized to the probe cable. Size the box for the
  **stacked** height, not the Nucleo alone.
- **RF:** keep the antenna clear of metal — see the keep-out in §1.
- **Test points:** the five in the BOM. Five pads turn a field failure into a
  30-second measurement.

---

## 7. Bring-up order

Do this before the boards go in a sealed box on a pole. The
[`bluepill_f103c8_dump`](../README.md) diagnostic is the right first power-on test
and ports to the WL55 in a few lines.

**Front-end alone, no Nucleo fitted** — this is why the test pads exist:

1. Continuity check `VBAT`/`GND` at the socket strips against the §2 table.
   Confirm the keyed position is blocked.
2. Bench supply on `VBAT`. Pull `SENS_GATE` high → `VSENS` = 0 V. Pull it low →
   `VSENS` = `VBAT`, both DQ idle high through their pull-ups.

**Stacked:**

3. Gate off → sleep current is µA.
4. Pin probe → `pull-up=1 pull-down=1` on both lines (proves the pull-up is really
   on `VSENS`, not GND).
5. Read probes → plausible independent temperatures, CRC OK.
6. Full cycle → wake, read, TX, sleep; confirm Stop2 current after the gate closes.

Steps 2 and 4 catch the resistor-to-GND class of fault — all-zero scratchpads that
pass CRC and decode as a convincing `0.00 °C`, which `ds18b20_read()` rejects
explicitly for this reason.

---

## References

- UM2592 — STM32WL Nucleo-64 board (MB1389) user manual: the authority on morpho
  pinout, solder bridges, and the ST-LINK power arrangement.
- Maxim/ADI **AN148** — guidelines for reliable long 1-Wire networks.
- DS18B20 datasheet — **VDD 3.0–5.5 V** is the constraint driving battery choice.
