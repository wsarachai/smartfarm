/*
 * gateway_config.h — COMMITTED config for lora-gateway (NUCLEO-WL55JC1).
 * Maps each node_id byte to the friendly device_id shown on the dashboard, and
 * sets the VCP UART baud. Nothing secret (raw LoRa needs no keys) -> no secrets.h.
 */
#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <stdint.h>

/* ST-LINK virtual COM port = LPUART1 (PA2 TX / PA3 RX). The bridge on the host
 * reads this at the same baud (see lora-gateway/bridge/). */
#define GW_UART_BAUD        115200

/* On-board LED pulsed on each valid RX (NUCLEO-WL55JC1 LED2 = PB9). */
#define GW_RX_LED_PORT      GPIOB
#define GW_RX_LED_PIN       GPIO_PIN_9
#define GW_RX_LED_CLK()     __HAL_RCC_GPIOB_CLK_ENABLE()

/*
 * node_id -> device_id. Add a row per node. The default (unknown id) falls back
 * to "water-node-<id>" so a new node still shows up on the dashboard.
 */
static inline const char *gw_device_id(uint8_t node_id)
{
    switch (node_id) {
        case 1:  return "water-temp-01";
        /* case 2: return "water-temp-02"; */
        default: return 0;   /* 0 -> caller formats "water-node-<id>" */
    }
}

#endif /* GATEWAY_CONFIG_H */
