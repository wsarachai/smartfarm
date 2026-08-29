/*
 * gateway_config.h — COMMITTED config for lora-gateway (NUCLEO-WL55JC1).
 * Maps each node_id byte to the friendly device_id shown on the dashboard,
 * names the temperature probes, and sets the VCP baud. Nothing secret (raw LoRa
 * needs no keys) -> no secrets.h.
 */
#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <stdint.h>

#include "lora/lora_packet.h"

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

/*
 * Probe index -> dashboard metric name.
 *
 * Probes 0 and 1 keep the names they have always had. That is deliberate: they
 * occupy the same wire slots they did when the node had exactly two probes, so
 * keeping the labels means the dashboard's existing `temp_hot` / `temp_cold`
 * history stays one continuous series across the upgrade instead of dead-ending
 * beside two new ones.
 *
 * Rename these to suit the install ("temp_inlet", "temp_tank_top", ...) — the
 * decode path does not care, and the web-server's telemetry schema is a generic
 * metrics dict, so new names need no server change. Just remember that renaming
 * a probe starts a NEW series; the old one stops rather than continues.
 *
 * Keep the array LORA_PROBE_MAX long.
 */
static inline const char *gw_probe_metric(int i)
{
    static const char *names[LORA_PROBE_MAX] = {
        "temp_hot",     /* probe 0 — v1 frame slot, historic name */
        "temp_cold",    /* probe 1 — v1 frame slot, historic name */
        "temp_p2",
        "temp_p3",
        "temp_p4",
        "temp_p5",
    };
    if (i < 0 || i >= LORA_PROBE_MAX) return "temp_unknown";
    return names[i];
}

/*
 * Air sensor index -> dashboard metric names. Same rule as the probes: sensor
 * 0 keeps the historic `air_temp` / `humidity` names because it occupies the
 * same wire slots it always did, so that dashboard history stays one
 * continuous series. Sensors 1 and 2 are new and get new names.
 *
 * Rename per install ("air_vent", "air_canopy", "air_floor", ...) — but a
 * renamed sensor starts a NEW series.
 */
static inline const char *gw_air_metric(int i)
{
    static const char *names[LORA_AIR_MAX] = {
        "air_temp",     /* sensor 0 — v2 frame slot, historic name */
        "air_temp_2",
        "air_temp_3",
    };
    if (i < 0 || i >= LORA_AIR_MAX) return "air_temp_unknown";
    return names[i];
}

static inline const char *gw_hum_metric(int i)
{
    static const char *names[LORA_AIR_MAX] = {
        "humidity",     /* sensor 0 — v2 frame slot, historic name */
        "humidity_2",
        "humidity_3",
    };
    if (i < 0 || i >= LORA_AIR_MAX) return "humidity_unknown";
    return names[i];
}

#endif /* GATEWAY_CONFIG_H */
