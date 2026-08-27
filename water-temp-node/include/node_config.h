/*
 * node_config.h — COMMITTED hardware + behaviour contract for water-temp-node.
 * Nothing secret lives here (raw point-to-point LoRa needs no keys), so unlike
 * the ESP nodes there is no secrets.h. Edit the pins/interval to match wiring.
 *
 * Board: NUCLEO-WL55JC1 (STM32WL55JC). Pins below deliberately avoid:
 *   PC3/PC4/PC5  RF switch control (radio)
 *   PA13/PA14    SWD (ST-LINK debug)
 *   PA2/PA3      LPUART1 = ST-LINK virtual COM (debug log, see DEBUG_UART)
 *   PB0          RF_TCXO_VCC (radio TCXO)
 */
#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

/* This node's id byte on the wire. The gateway maps it to a friendly device_id
 * (see lora-gateway/include/gateway_config.h). 1..255. */
#define NODE_ID                 1

/* Deep-sleep wake interval in seconds (Stop2 + RTC). 900 = 15 min. */
#define WAKE_INTERVAL_S         900

/* ---- DS18B20 1-Wire data pins (one probe each, SKIP ROM) ----------------- */
/* "hot" and "cold" are fixed by which pin the probe is plugged into. */
#define DS_HOT_PORT             GPIOA
#define DS_HOT_PIN              GPIO_PIN_10
#define DS_HOT_GPIO_CLK()       __HAL_RCC_GPIOA_CLK_ENABLE()

#define DS_COLD_PORT            GPIOA
#define DS_COLD_PIN             GPIO_PIN_9
#define DS_COLD_GPIO_CLK()      __HAL_RCC_GPIOA_CLK_ENABLE()

/* ---- Sensor rail power gate (A0341 P-MOSFET high-side, active-LOW) -------- */
/* LOW  = gate pulled low  = P-FET ON  = DS18B20 rail powered.
 * HIGH = gate = source     = P-FET OFF = rail off (also the Hi-Z sleep state,
 *        held OFF by the external 100k gate->3V3 pull-up). */
#define DS_PWR_PORT             GPIOA
#define DS_PWR_PIN              GPIO_PIN_8
#define DS_PWR_GPIO_CLK()       __HAL_RCC_GPIOA_CLK_ENABLE()
#define DS_PWR_ON_LEVEL         GPIO_PIN_RESET   /* active-low */
#define DS_PWR_OFF_LEVEL        GPIO_PIN_SET

/* Settling time after powering the rail before starting a conversion (ms). */
#define DS_POWER_SETTLE_MS      10

/* DS18B20 12-bit conversion time (ms). Drop to ~94 ms if you set 9-bit res. */
#define DS_CONVERT_MS           750

/* ---- I2C sensor bus (SHT45 + SCD41 share it) ------------------------------ */
/* I2C2 on PA11/PA12. Chosen over I2C1 (PA9/PA10 are the DS18B20 probes) and
 * I2C3 (PB10/PB11 -- PB11 is the Nucleo's LED3, which would sit on SDA). Both
 * parts hang off the SAME gated VSENS rail as the probes, so the whole sensor
 * front-end is still switched by one MOSFET. */
#define I2C_SDA_PIN             PA11
#define I2C_SCL_PIN             PA12
#define I2C_CLOCK_HZ            100000   /* 100 kHz: kind to a long, gated bus */

/* SHT45 (air temp + humidity). 0x45 on the -B variant. */
#define SHT45_ADDR              0x44

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
