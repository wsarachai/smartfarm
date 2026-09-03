/*
 * node_config.h — COMMITTED hardware + behaviour contract for water-temp-node.
 * Nothing secret lives here (raw point-to-point LoRa needs no keys), so unlike
 * the ESP nodes there is no secrets.h. Edit the pins/interval to match wiring.
 *
 * Board: NUCLEO-WL55JC1 (STM32WL55JC). The front-end is a LARGE CUSTOM PCB
 * joined to the Nucleo by a CABLE, so every logic signal below lives on the ST
 * morpho header **CN10** and is reachable in one 2x19 IDC ribbon; battery power
 * arrives separately at CN6. See docs/hardware-interface.md for the connector
 * contract, the CN10 pin layout, and the reasoning.
 *
 * Pins below deliberately avoid:
 *   PC3/PC4/PC5  RF switch control (radio)  -- CN10-38 / CN10-2 / CN10-4
 *   PB0          RF_TCXO_VCC (radio TCXO)   -- CN10-22
 *   PA13/PA14    SWD (ST-LINK debug)        -- CN7-13/15
 *   PA2/PA3      ST-LINK virtual COM -- NOT on the morpho at all, and the
 *                ST-LINK is unpowered anyway. Debug goes out on PB6/PB7.
 *   PA0/PA1/PC6  user buttons B1/B2/B3      -- switch + pull-up already fitted
 *   PB9/PB11/PB15 user LEDs LED2/LED3/LED1  -- an LED across a 1-Wire line ruins it
 * ALL of these are reachable on CN10, so the front-end must avoid them by
 * intent rather than by geometry -- see docs/hardware-interface.md.
 */
#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

/* This node's id byte on the wire. The gateway maps it to a friendly device_id
 * (see lora-gateway/include/gateway_config.h). 1..255. */
#define NODE_ID                 1

/* Deep-sleep wake interval in seconds (Stop2 + RTC). 900 = 15 min. */
#define WAKE_INTERVAL_S         900

/* ---- DS18B20 1-Wire data pins ------------------------------------------- *
 * ONE PROBE PER PIN, SKIP ROM — no shared bus and no ROM search. That is what
 * makes the driver's SKIP ROM legal, lets all six convert in PARALLEL (one
 * 750 ms wait, not six), and keeps a single shorted probe from taking the other
 * five down with it.
 *
 * Probes are identified by which connector they are plugged into: probe 0 is
 * whatever hangs off DQ_P0. Probes 0 and 1 land in the wire frame's original
 * two temperature slots and are published as `temp_hot` / `temp_cold` so an
 * existing dashboard's history stays continuous (lora_packet.h, v4).
 *
 * Pin order below is ASCENDING CN10 position, so the buzz-out in
 * docs/hardware-interface.md §7 walks the connector in one direction:
 *
 *   DQ_P0  PA5   CN10-11      DQ_P3  PC2   CN10-21
 *   DQ_P1  PA4   CN10-17      DQ_P4  PB8   CN10-27   <-- moved off PC1 (2026-08)
 *   DQ_P2  PA9   CN10-19      DQ_P5  PB10  CN10-25
 *
 * DQ_P4 moved from PC1 to PB8 in 2026-08 because PC1 was then a UART TX pin for
 * a CO2 sensor on RS-485. That subsystem is gone (2026-09: the CO2 part is an
 * SCD41 on I2C) and PC1 is free again -- but DQ_P4 STAYS on PB8. Moving it back
 * would churn the connector contract, the front-end layout and this table for
 * no gain. PB8 sits at CN10-27, flanked by the
 * two LED positions (26/28) which the front-end leaves open, so it keeps the
 * "every DQ has a guard neighbour" rule from docs/hardware-interface.md.
 *
 * Fewer than six probes fitted? Leave the table alone and simply do not plug
 * them in — an unconnected line reads no presence pulse and is transmitted as
 * the invalid sentinel, which the gateway drops. Set DS_PROBE_COUNT lower only
 * if you also want the ~7 ms/probe of bus traffic back.
 */
#define DS_PROBE_COUNT          6

/* Initializer for an array of ds_bus_t. Order defines the probe index. */
#define DS_PROBE_BUSES          { { GPIOA, GPIO_PIN_5  },   /* DQ_P0 */ \
                                  { GPIOA, GPIO_PIN_4  },   /* DQ_P1 */ \
                                  { GPIOA, GPIO_PIN_9  },   /* DQ_P2 */ \
                                  { GPIOC, GPIO_PIN_2  },   /* DQ_P3 */ \
                                  { GPIOB, GPIO_PIN_8  },   /* DQ_P4 */ \
                                  { GPIOB, GPIO_PIN_10 } }  /* DQ_P5 */

/* Every GPIO bank the table above (and the gate below) touches. */
#define DS_PROBE_GPIO_CLK()     do { __HAL_RCC_GPIOA_CLK_ENABLE(); \
                                     __HAL_RCC_GPIOB_CLK_ENABLE(); \
                                     __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)

/* ---- Sensor rail power gate (AO3401A P-MOSFET high-side, active-LOW) -------- */
/* LOW  = gate pulled low  = P-FET ON  = sensor rail powered.
 * HIGH = gate = source     = P-FET OFF = rail off (also the Hi-Z sleep state,
 *        held OFF by the external 100k gate->3V3 pull-up).
 *
 * PA8 (CN10-16) sits inside the DQ block, so the gate and the lines it powers
 * travel the same ribbon. Its alternate functions are unused here, and the WL
 * has no boot strap on it (BOOT0 is PH3, nBOOT1 is an option byte), so it
 * resets to a floating input -- which is exactly what the external 100k gate
 * pull-up needs in order to hold the rail off before firmware runs.
 *
 * NOTE the name: DS_PWR_* predates the SHT45s and gates the whole 3.3 V sensor
 * rail, not just the DS18B20 probes. The docs call this signal SENS_GATE.
 *
 * 2026-08: its PURPOSE changed. On the old battery node it existed to stop uA of
 * leakage mattering; on 24 V solar that is irrelevant. It is kept because it is
 * the only reliable way to recover a hung I2C bus -- three unshielded 5 m
 * branches in a greenhouse WILL latch SDA low eventually. On a failed
 * transaction: gate off, wait, gate on, retry.
 *
 * EVERYTHING the front-end reads is behind this gate, including the CO2 sensor:
 * the SCD41 takes on-demand single shots and is happiest power-cycled, so the
 * gate covers all four 5 m branches and there is no always-on sensor rail left
 * to reason about. (Not true before 2026-09, when the CO2 part needed 5 V and
 * continuous power -- a board with an ungated sensor rail is not this design.) */
#define DS_PWR_PORT             GPIOA
#define DS_PWR_PIN              GPIO_PIN_8
#define DS_PWR_GPIO_CLK()       __HAL_RCC_GPIOA_CLK_ENABLE()
#define DS_PWR_ON_LEVEL         GPIO_PIN_RESET   /* active-low */
#define DS_PWR_OFF_LEVEL        GPIO_PIN_SET

/* Settling time after powering the rail before starting a conversion (ms). */
#define DS_POWER_SETTLE_MS      10

/* DS18B20 12-bit conversion time (ms). Drop to ~94 ms if you set 9-bit res.
 * Unchanged by the probe count: the probes convert in parallel. */
#define DS_CONVERT_MS           750

/* ---- I2C sensor bus (three SHT45s + the SCD41, all remote) --------------- */
/* I2C2 on PA11/PA12 (CN10-5 / CN10-3). Chosen over I2C1 (PA9/PA10 -- PA9 is a
 * probe) and I2C3 (PB10/PB11 -- PB10 is a probe and PB11 is the Nucleo's LED3,
 * which would sit on SDA).
 *
 * On the bus: the TCA9548A, three SHT45s on channels 0-2, and the SCD41 on
 * channel 3. None of them are on the board -- each sits at the far end of its
 * own <=5 m cable, which is why the downstream pull-ups are 2.2k rather than
 * 4.7k. All on the gated VSENS rail. */
#define I2C_SDA_PIN             PA11
#define I2C_SCL_PIN             PA12
#define I2C_CLOCK_HZ            100000   /* 100 kHz: kind to four 5 m branches */

/* ---- SHT45 air temp + humidity: THREE of them ---------------------------
 * The SHT4x I2C address is fixed at the factory by the order code
 * (SHT45-AD1B = 0x44). There is no address pin, so three of them CANNOT share
 * a bus, and power-gating them individually does not help either -- an
 * unpowered part still clamps SDA/SCL to its own VDD through its ESD diodes
 * and phantom-powers itself to ~2.7 V, where it happily answers at 0x44.
 *
 * So the three sit behind a TCA9548A bus switch, one channel connected at a
 * time. The SCD41 (0x62) does not collide with 0x44 and could in principle hang
 * upstream of the switch -- but it must NOT, because it is 5 m away: an
 * unswitched branch puts its ~320 pF on the bus permanently, which is the exact
 * load-stacking the mux exists to prevent. It gets channel 3 instead.
 *
 * With every channel closed an I2C scan should find 0x70 and nothing else;
 * 0x44 on channels 0-2 and 0x62 on channel 3. See src/tca9548a.h and
 * docs/hardware-interface.md.
 */
#define SHT45_COUNT             3
#define SHT45_ADDR              0x44   /* all three -- factory-fixed */

/* TCA9548A bus switch: address (A2/A1/A0 strapping) and the channel each
 * SHT45 hangs off. Index == sensor index in the LoRa frame and on the
 * dashboard, so sensor 0 is whatever is on I2C_MUX_CHANNELS[0].
 *
 * 2026-08: each channel now drives its OWN <=5 m cable to a sensor somewhere
 * else. The mux is what makes that work -- only the selected branch's ~320 pF
 * loads the bus, so three 5 m branches never add up.
 *
 *   ch0 -> SHT45 #0  head of the greenhouse   (frame slot 0, historic series)
 *   ch1 -> SHT45 #1  tail of the greenhouse   (frame slot 1)
 *   ch2 -> SHT45 #2  OUTSIDE, ambient ref     (frame slot 2, radiation shield)
 *
 * Swapping two branch cables silently relabels the data and it still looks
 * plausible -- label both ends. */
#define I2C_MUX_ADDR            0x70
#define I2C_MUX_CHANNELS        { 0, 1, 2 }

/* ---- CO2: Sensirion SCD41 on I2C, behind the mux ------------------------- *
 * What it costs the board: one mux channel and one connector. It needs no
 * regulator of its own, no transceiver, and no extra signal across the
 * board-to-board joint.
 *
 * Why this part:
 *   - 2.4-5.5 V, so it runs from the same 3.3 V as everything else.
 *   - I2C with a CRC-8 on EVERY 16-bit word (sensirion_i2c.cpp), so a corrupted
 *     reply is detected and retried rather than published as a plausible wrong
 *     number. I2C here costs no data integrity against a framed protocol.
 *   - measure_single_shot: on-demand measurement, so it lives behind SENS_GATE
 *     like everything else instead of needing an always-on rail.
 *   - -10..60 C, 0-95 %RH. The mushroom house -- warm, and at 90-95 %RH after
 *     misting -- is what made this range a hard requirement rather than a
 *     preference, and it is why the CO2 part was changed in 2026-09.
 *
 * The cost is current: 175 mA typ / 205 mA max peak, ten times its own average
 * and ten times anything else the front-end powers. Two consequences, both
 * handled:
 *   - the 5 m branch is 22 AWG with 100 uF (low-ESR) + 100 nF at the head, so
 *     the ~109 mV IR drop lands well above the 2.4 V minimum. That capacitor is
 *     also the shunt leg of the filter that gets U7's 75 mVpp switching ripple
 *     under the SCD41's 30 mV supply limit -- see docs/hardware-interface.md.
 *   - main.cpp reads it LAST inside the gated window, after the DS18B20
 *     conversions have finished, so its bursts never overlap theirs.
 *
 * History, because it explains the pin map above: the CO2 sensor was on RS-485
 * until 2026-09, and deleting that subsystem removed U4, U5, the head LDO, the
 * A/B TVS pairs, the 5 V rail and its buck module, plus three signals from the
 * board-to-board connector (PC1, PC0, PA7 -- now free and unused). That is why
 * the contract dropped from eighteen signals to fifteen.
 */
#define SCD41_ADDR              0x62
#define SCD41_MUX_CHANNEL       3        /* SHT45s hold 0..2 -- see above */

/* ASC (Automatic Self-Calibration) is ENABLED at the factory and is WRONG here.
 * It assumes weekly exposure to 400 ppm outdoor air and calibrates the lowest
 * reading it has seen toward SCD41_ASC_TARGET. A mushroom house never reaches
 * 400 ppm, so ASC would drag every reading down, the controller would believe
 * the air is fine, and it would under-ventilate -- the exact failure this
 * sensor is fitted to prevent, and invisible on the dashboard.
 *
 * So: OFF, enforced on every wake (a 1 ms read; EEPROM is only written when the
 * sensor actually disagrees), and recalibrated by hand with FRC in outdoor air
 * at each crop changeover. See src/scd41.h and docs/hardware-interface.md §3. */
#define SCD41_ASC_ENABLED       0
#define SCD41_FRC_TARGET_PPM    420      /* outdoor air, for the service FRC */

/* Altitude for the sensor's own density compensation, in metres.
 * set_ambient_pressure lives in RAM and would be lost every time the gate drops;
 * the altitude setting is EEPROM-backed, so it is written once and survives the
 * power cycling. Maejo is ~330 m. 0 disables the compensation. */
#define SCD41_SITE_ALTITUDE_M   330

/* ---- Sensor rail settle time for the I2C parts --------------------------- */
/* This was 1000 ms for years, on the strength of an SCD41 power-up figure that
 * Sensirion has since corrected -- SCD4x v1.7 Table 7 says 30 ms max. The
 * sequencing built around overlapping that 1 s with the 750 ms DS18B20 conversion
 * survives anyway, because it is the right shape and now costs nothing. Every I2C
 * part here settles in single-digit ms: the SHT45s ~1 ms, the mux immediately,
 * the SCD41 30 ms. Rail-on time is set by DS_CONVERT_MS.
 *
 * 2026-09: the SCD41 needs a settle window again (SCD41_POWER_UP_MS), but it
 * costs nothing -- it is addressed last, by which time the rail has been up for
 * the whole 750 ms conversion plus three SHT45 reads, against the datasheet's
 * 30 ms power-up. (The driver carried 1000 ms until 2026-09. That came from
 * datasheet v1.3, which really did say 1000 ms; v1.4 corrected it to 30 ms and
 * v1.7 still does -- see src/scd41.h.) */
#define I2C_POWER_SETTLE_MS     10

/* ---- Supply telemetry: 24 V bank via a divider ---------------------------- *
 * battery_read_mv()'s VREFINT trick is DEAD. It measured VDDA, which is now the
 * 3.3 V buck output -- the same number whether the bank is full or flat.
 *
 * Replacement: a 300k/30k divider (11:1) off the 24 V input to PB3 = ADC1_IN2,
 * the ONLY ADC-capable pin left on CN10. 32 V in -> 2.9 V at the pin.
 *
 * VREFINT is still read, for a different job: recover the ACTUAL VDDA so the
 * divider reading can be scaled against it. Otherwise the buck's tolerance shows
 * up as error on every reported bank voltage. */
#define VBAT_SENSE_PIN          PB3
#define VBAT_SENSE_NUM          300000UL  /* R24, ohms */
#define VBAT_SENSE_DEN          30000UL   /* R25, ohms */
#define VBAT_SENSE_SAMPLE_US    100       /* 300k||30k = 27k source impedance */

/* ---- Debug UART (USART1 -> 3-pin header J13, NOT the ST-LINK VCP) --------- *
 * The VCP is gone: feeding 3V3 directly at CN6-4 leaves the ST-LINK unpowered,
 * so PA2/PA3 reach nothing. USART1 on PB6/PB7 (CN10-35/37) goes out to a 3-pin
 * header instead and you attach a USB-serial adapter. Those two positions were
 * marked forbidden while they were assumed to be the VCP; they are not -- PA2/PA3
 * are not on the morpho at all. Comment out to drop all serial logging. */
#define DEBUG_UART_ENABLED      1
#define DEBUG_UART_TX_PIN       PB6
#define DEBUG_UART_RX_PIN       PB7
#define DEBUG_UART_BAUD         115200

/* Software bound on a single TX attempt (ms) before we give up and sleep. */
#define LORA_TX_TIMEOUT_MS      4000

#endif /* NODE_CONFIG_H */
