# Sourcing from Thailand — water-temp-node front-end

Manufacturer part numbers for everything in `hardware-interface.md` §4 that is not
a generic passive. **Search these MPNs on [digikey.co.th](https://www.digikey.co.th)** —
Digi-Key TH ships from US stock with THB pricing and DDP customs.

> **Stock and price are not recorded here on purpose.** They move weekly and a
> stale number in a spec file is worse than no number. Check before ordering, and
> check the *alternates* column before assuming something is unobtainable.

> **Looking for a shopping list rather than substitution rules?** See
> [`parts-list-th.md`](parts-list-th.md) — every item for one node plus a gateway,
> numbered, with quantities, spares and purchase waves, covering the two
> prototype stages as well as the final board. This file stays the authority on
> *what may be substituted for what*; that one is the authority on *how many*.

## The parts that actually constrain the design

| Ref | MPN | What to check when substituting |
|---|---|---|
| **CO2** | Sensirion **SCD41-D-R2** (Digi-Key TH ฿673.73) — or the **`SEK-SCD41-SENSOR`** breakout (also Digi-Key TH), which is the **recommended** form for this build | I2C 0x62, 2.4–5.5 V, **−10–60 °C / 0–95 %RH**, single-shot capable, CRC-8 on every word. Replaced the Senseair S88 LP in revision 2.0 because a mushroom house sits at 90–95 %RH and the S88 is an 85 %RH part. The SCD41 is **reflow-only (LGA 10.1 × 10.1 mm, MSL 1)** — hence the breakout. **Peak 205 mA**: its 5 m branch is **22 AWG**, not the 24 AWG used for the SHT45s. If the CO2 range requirement ever grows past 5000 ppm, the fallback is the **Senseair S88 GH, 004-1-0102** (฿896.64) — but it brings back 5 V and RS-485; see `hardware-interface-s88.md` |
| **U1a–c** | Sensirion **SHT45-AD1B** | `-AD1B` is the **0x44** order code. `-BD1B` is 0x45 and would break `SHT45_ADDR`. DFN-4, 1.5×1.5 mm — hand-solderable but not pleasant |
| **U3** | TI **TCA9548APWR** (TSSOP-24) | Or **PCA9548APW**. Pin *numbers* differ between TSSOP and QFN — take them from the datasheet for the package you buy |
| **U7** | Traco Power **TSR 1-2433** | Same series, **3.3 V ±2 %** (3.23–3.37 V worst case, under the 3.6 V ceiling of CN6-4). 4.75–36 V in, 1 A. Digi-Key TH ฿206.49 / 10 k in stock (2026-09-02). Alternates: Pololu **D36V6F3**, Murata **OKI-78SR-3.3/1.5-W36-C**. **Never an adjustable module** — the ±2 % factory trim is what protects the MCU |
| ~~L1, L2~~ | — | **Deleted in revision 1.0** together with the LM5164 network (C13, C16–C18, C20, C22–C25, R26–R37). The inductors and every COT value live on in `hardware-interface-back.md` |
| **C16, C22** | 2.2 nF, **50 V X7R**, 0603 | `C_BST`. TI specifies 1.5–2.5 nF as an absolute maximum-ratings entry, not a suggestion — a bigger cap stresses the internal VCC regulator and damages the device |
| **Q1** | AOS **AO3401A** (SOT-23) | Select on **R_DS(on) at V_GS = −2.5 V**, not the headline −4.5 V. Alternates: **DMG3415U**, **SI2301CDS** |
| **Q2** | Diodes **DMP6023LE-13** (SOT-223) | Reverse-polarity on the 24 V input. −60 V, 7 A, 28 mΩ @ V_GS = −10 V, **tab = drain**. Substitutes need V_DS ≥60 V and R_DS(on) specified at −10 V — do **not** reuse the AO3401A, it is a 30 V part. Whatever you fit, check **V_GS(max)**: at ±20 V the gate clamp `D10` is mandatory, and a part rated ±12 V would need a lower-voltage Zener |
| **D10** | 12 V Zener, 250 mW — **BZX84C12** (SOT-23) or **MMSZ5242B** (SOD-123) | Q2's gate clamp. **Not** interchangeable with a TVS. Stay at 12 V: 16 V or 18 V leaves too little margin against Q2's ±20 V gate limit at a 32 V bank |
| **D9** | Littelfuse **SMBJ33A** | 33 V standoff / 600 W, on the 24 V input. Unidirectional; the bus never goes negative once Q2 is in |
| **D1–D6** | Bourns **CDSOD323-T05LC** | 5 V, 1 pF, SOD-323. Budget alternate: onsemi **ESD9B5.0ST5G** (15 pF, SOD-923 — 0.8×0.6 mm, fine for assembly, unpleasant by hand). **Reject** anything whose datasheet buries the capacitance figure |
| **F1** | 2 A **time-lag (T)**, 5×20 mm cartridge + holder — **prefer this over the 1206** | On the 24 V input, before Q2. Specify by **I²t ≥0.5 A²s**, not by amps: the hot-plug inrush through Q2's body diode is ~0.1 A²s and some 1206 "slow-blow" 2 A parts sit right on that number. A cartridge is also the one you can replace on a pole |

## Connectors

| Ref | MPN | Notes |
|---|---|---|
| J1–J6 | Phoenix **MC 1,5/3-ST-3,5** (1840379) + matching header | 3-pin pluggable, one per probe. Clones from JST/Degson are fine and much cheaper locally |
| J9–J12 | Phoenix **MC 1,5/4-ST-3,5** (1840382) + header | 4-pin: three SHT45 branches **and the SCD41 head** — four identical connectors, one part number, one cable pinout `V / SDA / SCL / G` |
| J7 | 2×19 shrouded boxed header, 2.54 mm + 2× IDC socket + 38-way ribbon | **Clip CN10-6 and plug the matching socket hole** — the keying trick in §1 |
| J8 | JST **XHP-2** / Micro-Fit 3.0, keyed | 3.3 V to Nucleo CN6-4/6 |
| J13 | 1×3 pin header, 2.54 mm | Debug UART for a USB-serial adapter |
| J14 | 2-pin, ≥5 A, keyed | 24 V solar input |

## Local Thai suppliers — usually faster and cheaper for the common parts

Digi-Key TH is the right source for the SCD41, the SHT45s, the TI parts and the
Bourns TVS. For passives, connectors, ribbon, enclosures, glands and DS18B20
probes, local shops are typically same-week and much cheaper:

- **ThaiEasyElec** (thaieasyelec.com) — sensors, modules, connectors
- **Gravitech Thailand** (gravitech.co.th) — Arduino-ecosystem parts, headers
- **ArduinoAll / Cytron TH** — DS18B20 stainless probes, Cat5, enclosures
- **Ban Mo / คลองถม** electronics markets, Bangkok — fuses, glands, IP65 boxes

**Waterproof DS18B20 probes** (stainless, 3-wire, pre-potted) are a local-supplier
item — they are cheap, and buying six identical ones from one batch matters more
than the brand. **The 100 nF at each probe is not included** with them; buy those
separately and fit them at the sensor end (§3).

## What to buy first, given the staged plan

The CO2 sensor is fitted **last**, after the SHT45 logs show what the mushroom
house actually does at mid-house — temperature, humidity and above all **dew
point** (§3). So order in two waves:

1. **Now:** everything except the SCD41 and its head parts. Build the board
   complete — including **J12, R39/R40 and the channel-3 branch** — so nothing
   needs rework. Revision 2.0 makes this wave cheaper than it used to be: no U4,
   no U5, no head LDO, no A/B TVS pair, and no 5 V module.
2. **After the logs:** the SCD41 (or the SEK breakout), its 100 µF + 100 nF, the
   PTFE/Gore membrane, the vented splash-proof housing — and **the heater
   resistor, only if the dew-point logs say so** (§3, *Condensation*). Lay its pad
   out in wave 1 either way.

The register map is the **Sensirion SCD4x datasheet v1.7** and `src/scd41.{h,cpp}`
already implements it — the driver predates this revision and was hardware-verified
on the F103 prototype. Two settings must be right before the data means anything,
and both are enforced by firmware on every wake: **ASC off** (the factory default
is ON, and it is actively harmful in a house that never sees 400 ppm) and
**`SCD41_SITE_ALTITUDE_M`** (see `hardware-interface.md` §3
*Pressure*).
