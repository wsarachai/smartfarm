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
| **U6, U7** | TI **LM5164DDAR** | 100 V, 1 A synchronous buck, ~10 µA I_q. **One part number for both rails** — only the feedback divider differs (5.1 V vs 3.3 V). Alternates rated ≥60 V: **LM5160ADNTR**, **LMR51440**. Avoid the popular 28 V parts (TPS54202, MP1584): a 24 V bank hits 28.8 V on absorb and you have no margin |
| **Q1** | AOS **AO3401A** (SOT-23) | Select on **R_DS(on) at V_GS = −2.5 V**, not the headline −4.5 V. Alternates: **DMG3415U**, **SI2301CDS** |
| **Q2** | P-FET, **≥60 V**, R_DS(on) ≤100 mΩ, SOT-223 or DPAK | Reverse-polarity on the 24 V input. Filter Digi-Key on V_DS ≥60 V — do **not** reuse the AO3401A here, it is a 30 V part |
| **D9** | Littelfuse **SMBJ33A** | 33 V standoff / 600 W, on the 24 V input. Unidirectional; the bus never goes negative once Q2 is in |
| **D1–D6** | Bourns **CDSOD323-T05LC** | 5 V, 1 pF, SOD-323. Budget alternate: onsemi **ESD9B5.0ST5G** (15 pF, SOD-923 — 0.8×0.6 mm, fine for assembly, unpleasant by hand). **Reject** anything whose datasheet buries the capacitance figure |
| **D7, D8** | same family as D1–D6 | On RS-485 `A`/`B`. ±12 V standoff is fine here — these are not 3.3 V logic lines |
| LDO (head) | **AP2112K-3.3TRG1** or **MCP1700T-3302E/TT** | Powers U5 off the 5 V rail so `DVCC_out` stays unloaded (datasheet: 6 mA max, and loading it "may affect sensor performance") |
| **F1** | 2 A slow-blow, 5×20 mm holder or 1206 | On the 24 V input, before Q2 |

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
