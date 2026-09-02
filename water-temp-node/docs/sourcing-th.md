# Sourcing from Thailand — water-temp-node front-end

Manufacturer part numbers for everything in `hardware-interface.md` §4 that is not
a generic passive. **Search these MPNs on [digikey.co.th](https://www.digikey.co.th)** —
Digi-Key TH ships from US stock with THB pricing and DDP customs.

> **Stock and price are not recorded here on purpose.** They move weekly and a
> stale number in a spec file is worse than no number. Check before ordering, and
> check the *alternates* column before assuming something is unobtainable.

## The parts that actually constrain the design

| Ref | MPN | What to check when substituting |
|---|---|---|
| **CO2** | Senseair **S88 LP, art. 004-1-0101** | This is the part the design is built around. UART/Modbus, 4.5–5.25 V, 300 mA peak. Senseair also sells the **S8 LP** (004-0-0053) — similar interface, different register map and footprint, **not** a drop-in |
| **U1a–c** | Sensirion **SHT45-AD1B** | `-AD1B` is the **0x44** order code. `-BD1B` is 0x45 and would break `SHT45_ADDR`. DFN-4, 1.5×1.5 mm — hand-solderable but not pleasant |
| **U3** | TI **TCA9548APWR** (TSSOP-24) | Or **PCA9548APW**. Pin *numbers* differ between TSSOP and QFN — take them from the datasheet for the package you buy |
| **U4** | TI **THVD1450DR** (SOIC-8) | 3.3 V, true-failsafe, ±18 kV ESD. Alternates: **MAX3485ESA+**, **SP3485EN-L**, **SN65HVD3082EDR**. **Do not** substitute an auto-direction part without checking its supply voltage — MAX13487E is 5 V and its `RO` would drive 5 V into a non-5V-tolerant STM32WL pin |
| **U5** (head) | same as U4 | `DE`+`!RE` tied to the S88's `UART_R/T`. Classic **MAX485CSA+** works here too *if* you power it from 5 V and accept 5 V logic — but the S88's UART is 3.3 V-referenced, so a 3.3 V part is the cleaner choice |
| **U6** | Traco Power **TSR 1-2450** | **Revision 1.0 (2026-09): a module, not a chip.** 6.5–36 V in, 5 V ±2 %, 1 A, SIP-3 (78xx footprint: 1 = +V_in, 2 = GND, 3 = +V_out), 1 mA typ no-load, Digi-Key TH ฿206.49 / 26 k in stock (2026-09-02). Fixed output only. **Needs a 22 µF/50 V input capacitor** (Traco requirement above 32 V_in) — that is C12/C19. **Any substitute must be rated ≥36 V input**: a 24 V bank hits 28.8 V on absorb (29.2 V for 8S LiFePO4), so the popular 28 V modules — DFRobot DFR0571, MP1584, LM2596 boards — are out. Same-class alternates: Pololu **D36V6F5** (50 V, 600 mA, ±4 % — check the S88's 4.5 V minimum after cable drop), Murata **OKI-78SR-5/1.5-W36-C** |
| **U7** | Traco Power **TSR 1-2433** | Same series, **3.3 V ±2 %** (3.23–3.37 V worst case, under the 3.6 V ceiling of CN6-4). 4.75–36 V in, 1 A. Digi-Key TH ฿206.49 / 10 k in stock (2026-09-02). Alternates: Pololu **D36V6F3**, Murata **OKI-78SR-3.3/1.5-W36-C**. **Never an adjustable module** — the ±2 % factory trim is what protects the MCU |
| ~~L1, L2~~ | — | **Deleted in revision 1.0** together with the LM5164 network (C13, C16–C18, C20, C22–C25, R26–R37). The inductors and every COT value live on in `hardware-interface-back.md` |
| **C16, C22** | 2.2 nF, **50 V X7R**, 0603 | `C_BST`. TI specifies 1.5–2.5 nF as an absolute maximum-ratings entry, not a suggestion — a bigger cap stresses the internal VCC regulator and damages the device |
| **Q1** | AOS **AO3401A** (SOT-23) | Select on **R_DS(on) at V_GS = −2.5 V**, not the headline −4.5 V. Alternates: **DMG3415U**, **SI2301CDS** |
| **Q2** | Diodes **DMP6023LE-13** (SOT-223) | Reverse-polarity on the 24 V input. −60 V, 7 A, 28 mΩ @ V_GS = −10 V, **tab = drain**. Substitutes need V_DS ≥60 V and R_DS(on) specified at −10 V — do **not** reuse the AO3401A, it is a 30 V part. Whatever you fit, check **V_GS(max)**: at ±20 V the gate clamp `D10` is mandatory, and a part rated ±12 V would need a lower-voltage Zener |
| **D10** | 12 V Zener, 250 mW — **BZX84C12** (SOT-23) or **MMSZ5242B** (SOD-123) | Q2's gate clamp. **Not** interchangeable with a TVS. Stay at 12 V: 16 V or 18 V leaves too little margin against Q2's ±20 V gate limit at a 32 V bank |
| **D9** | Littelfuse **SMBJ33A** | 33 V standoff / 600 W, on the 24 V input. Unidirectional; the bus never goes negative once Q2 is in |
| **D1–D6** | Bourns **CDSOD323-T05LC** | 5 V, 1 pF, SOD-323. Budget alternate: onsemi **ESD9B5.0ST5G** (15 pF, SOD-923 — 0.8×0.6 mm, fine for assembly, unpleasant by hand). **Reject** anything whose datasheet buries the capacitance figure |
| **D7, D8** | same family as D1–D6 | On RS-485 `A`/`B`. ±12 V standoff is fine here — these are not 3.3 V logic lines |
| LDO (head) | **AP2112K-3.3TRG1** or **MCP1700T-3302E/TT** | Powers U5 off the 5 V rail so `DVCC_out` stays unloaded (datasheet: 6 mA max, and loading it "may affect sensor performance") |
| **F1** | 2 A **time-lag (T)**, 5×20 mm cartridge + holder — **prefer this over the 1206** | On the 24 V input, before Q2. Specify by **I²t ≥0.5 A²s**, not by amps: the hot-plug inrush through Q2's body diode is ~0.1 A²s and some 1206 "slow-blow" 2 A parts sit right on that number. A cartridge is also the one you can replace on a pole |

## Connectors

| Ref | MPN | Notes |
|---|---|---|
| J1–J6 | Phoenix **MC 1,5/3-ST-3,5** (1840379) + matching header | 3-pin pluggable, one per probe. Clones from JST/Degson are fine and much cheaper locally |
| J9–J12 | Phoenix **MC 1,5/4-ST-3,5** (1840382) + header | 4-pin: three SHT45 branches + the S88 head |
| J7 | 2×19 shrouded boxed header, 2.54 mm + 2× IDC socket + 38-way ribbon | **Clip CN10-6 and plug the matching socket hole** — the keying trick in §1 |
| J8 | JST **XHP-2** / Micro-Fit 3.0, keyed | 3.3 V to Nucleo CN6-4/6 |
| J13 | 1×3 pin header, 2.54 mm | Debug UART for a USB-serial adapter |
| J14 | 2-pin, ≥5 A, keyed | 24 V solar input |

## Local Thai suppliers — usually faster and cheaper for the common parts

Digi-Key TH is the right source for the S88, the SHT45s, the TI parts and the
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

The S88 is fitted **last**, after the SHT45 logs prove the greenhouse is inside
0–50 °C / ≤85 %RH (§3). So order in two waves:

1. **Now:** everything except the S88 and its head parts (U5, the head LDO, the
   head connector's mating plug). Build the board complete — including U4, J12 and
   the 5.1 V rail — so nothing needs rework.
2. **After the logs:** the S88 LP, U5, the head LDO, the radiation shield and the
   PTFE/Gore membrane.

Also get **TDE14367 "Modbus on Senseair S88"** from Senseair before wave 2 — it has
the register map, which is **not** in the product spec, and `S88_MODBUS_ADDR` /
`S88_CO2_REG` in `node_config.h` are placeholders until you have it.
