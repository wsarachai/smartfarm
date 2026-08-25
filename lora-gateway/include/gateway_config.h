/*
 * gateway_config.h — COMMITTED config for lora-gateway (NUCLEO-WL55JC1).
 * Maps each node_id byte to the friendly device_id shown on the dashboard, and
 * sets the VCP baud. Nothing secret (raw LoRa needs no keys) -> no secrets.h.
 */
#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <stdint.h>

/* ST-LINK virtual COM port baud. The host bridge reads at the same rate. */
#define GW_UART_BAUD        115200

/*
 * node_id -> device_id. Add a row per node. Returns 0 for an unknown id, and the
 * caller formats "water-node-<id>" so a new node still shows up on the dashboard.
 */
static inline const char *gw_device_id(uint8_t node_id)
{
    switch (node_id) {
        case 1:  return "water-temp-01";
        /* case 2: return "water-temp-02"; */
        default: return 0;
    }
}

#endif /* GATEWAY_CONFIG_H */
