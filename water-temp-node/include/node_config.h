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

/* ---- Debug UART (optional, LPUART1 on the ST-LINK VCP) -------------------- */
/* Comment out to drop all serial logging (saves a few uA + code). */
#define DEBUG_UART_ENABLED      1
#define DEBUG_UART_BAUD         115200

/* Software bound on a single TX attempt (ms) before we give up and sleep. */
#define LORA_TX_TIMEOUT_MS      4000

#endif /* NODE_CONFIG_H */
