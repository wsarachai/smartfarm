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
 *   PA2/PA3      LPUART1 = ST-LINK virtual COM (debug log, see DEBUG_UART)
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
 *   DQ_P1  PA4   CN10-17      DQ_P4  PC1   CN10-23
 *   DQ_P2  PA9   CN10-19      DQ_P5  PB10  CN10-25
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
                                  { GPIOC, GPIO_PIN_1  },   /* DQ_P4 */ \
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
 * NOTE the name: DS_PWR_* predates the SHT45/SCD41 and now gates the WHOLE
 * sensor rail, not just the DS18B20 probes. The docs call this signal SENS_GATE. */
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

/* ---- I2C sensor bus (SHT45 + SCD41 share it) ------------------------------ */
/* I2C2 on PA11/PA12 (CN10-5 / CN10-3). Chosen over I2C1 (PA9/PA10 — PA9 is a
 * probe) and I2C3 (PB10/PB11 — PB10 is a probe and PB11 is the Nucleo's LED3,
 * which would sit on SDA). Both parts hang off the SAME gated VSENS rail as the
 * probes, so the whole sensor front-end is still switched by one MOSFET. */
#define I2C_SDA_PIN             PA11
#define I2C_SCL_PIN             PA12
#define I2C_CLOCK_HZ            100000   /* 100 kHz: kind to a long, gated bus */

/* ---- SHT45 air temp + humidity: THREE of them ---------------------------
 * The SHT4x I2C address is fixed at the factory by the order code
 * (SHT45-AD1B = 0x44). There is no address pin, so three of them CANNOT share
 * a bus, and power-gating them individually does not help either -- an
 * unpowered part still clamps SDA/SCL to its own VDD through its ESD diodes
 * and phantom-powers itself to ~2.7 V, where it happily answers at 0x44.
 *
 * So the three sit behind a TCA9548A bus switch, one channel connected at a
 * time. The SCD41 (0x62, no collision) stays UPSTREAM of the switch. See
 * src/tca9548a.h and docs/hardware-interface.md.
 */
#define SHT45_COUNT             3
#define SHT45_ADDR              0x44   /* all three -- factory-fixed */

/* TCA9548A bus switch: address (A2/A1/A0 strapping) and the channel each
 * SHT45 hangs off. Index == sensor index in the LoRa frame and on the
 * dashboard, so sensor 0 is whatever is on I2C_MUX_CHANNELS[0]. */
#define I2C_MUX_ADDR            0x70
#define I2C_MUX_CHANNELS        { 0, 1, 2 }

/* SCD41 (CO2). */
#define SCD41_ADDR              0x62

/*
 * CO2 measurement is the expensive part of a wake, so it is optional and
 * separately paced. Comment out CO2_ENABLED to drop the SCD41 entirely.
 *
 * CO2_EVERY_N_WAKES: measure CO2 only on every Nth wake, sending the last known
 * value in between (flag stays set). 1 = every wake. At WAKE_INTERVAL_S=900 a
 * value of 4 gives one CO2 reading per hour, which is all a greenhouse trend
 * needs, and cuts the sensor's share of the battery budget by 4x.
 *
 * See README, "CO2 on a battery node", for the arithmetic behind this.
 */
#define CO2_ENABLED             1
#define CO2_EVERY_N_WAKES       4

/* Discard one measurement before the real one (datasheet: the first single shot
 * after power-up is unsettled). Costs a second SCD41_SINGLE_SHOT_MS. Turning it
 * off halves the CO2 energy cost and makes the readings worse -- do not, unless
 * you have left the sensor permanently powered. */
#define CO2_SINGLE_SHOT_WARMUP  1

/* ---- Sensor rail settle time for the I2C parts --------------------------- */
/* The SCD41 wants 1000 ms after VDD before it will accept a command -- 100x the
 * DS18B20's DS_POWER_SETTLE_MS. We start the 1-Wire conversions first and spend
 * this wait usefully, so it costs MCU-awake time but no extra rail-on time. */
#define I2C_POWER_SETTLE_MS     1000

/* ---- Debug UART (optional, LPUART1 on the ST-LINK VCP) -------------------- */
/* Comment out to drop all serial logging (saves a few uA + code). */
#define DEBUG_UART_ENABLED      1
#define DEBUG_UART_BAUD         115200

/* Software bound on a single TX attempt (ms) before we give up and sleep. */
#define LORA_TX_TIMEOUT_MS      4000

#endif /* NODE_CONFIG_H */
