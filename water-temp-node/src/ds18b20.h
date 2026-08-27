/*
 * ds18b20.h — bit-banged 1-Wire DS18B20 driver. One probe per pin (SKIP ROM),
 * so no ROM search. The data line is an open-drain GPIO with an external 4.7k
 * pull-up on the (power-gated) sensor rail. Timing uses delay_us() (DWT).
 */
#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
/* <Arduino.h> pulls in the correct per-family HAL (stm32wlxx / stm32f1xx ...),
 * so this driver is portable across STM32duino boards — the WL55 node and the
 * F103C8T6 bring-up test both use it unchanged. */
#include <Arduino.h>

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} ds_bus_t;

/* Configure the pin as open-drain output (released = high via ext. pull-up). */
void ds18b20_init(const ds_bus_t *bus);

/* Kick off a 12-bit temperature conversion on the single probe on this bus.
 * Returns 1 if a device responded to the reset (presence pulse), else 0.
 * The caller must wait DS_CONVERT_MS before ds18b20_read(). */
int ds18b20_start_convert(const ds_bus_t *bus);

/*
 * Read the completed conversion. On success writes centi-degC to *out_c100
 * (e.g. 41.30 C -> 4130) and returns 1. Returns 0 on no-presence or CRC fail.
 */
int ds18b20_read(const ds_bus_t *bus, int16_t *out_c100);

/*
 * Bring-up diagnostic: read the raw 9-byte scratchpad with NO validation.
 * Returns 1 if a presence pulse was seen (sp[] then holds whatever the bus
 * returned), 0 if nothing answered the reset. Use it to tell a wiring fault
 * apart from a bad probe: 0xFF.. = line never pulled low (no device driving),
 * 0x00.. = line stuck low, mixed garbage = two devices answering SKIP ROM on
 * one wire or bad slot timing.
 */
int ds18b20_read_scratchpad(const ds_bus_t *bus, uint8_t sp[9]);

#endif /* DS18B20_H */
