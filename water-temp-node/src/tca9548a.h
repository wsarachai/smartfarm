/*
 * tca9548a.h — 8-channel I2C bus switch (TI TCA9548A / NXP PCA9548A).
 *
 * Why this part is here at all: the node carries THREE SHT45s, and the SHT4x
 * I2C address is fixed at the factory by the order code (SHT45-AD1B = 0x44).
 * There is no address pin to strap, so three of them cannot share a bus.
 *
 * Power-gating them individually does NOT solve it. An unpowered SHT45 still has
 * ESD clamp diodes from SDA/SCL to its own VDD pin, so the 4.7 k bus pull-ups
 * charge the gated-off part's rail to about 3.3 - 0.6 = 2.7 V through those
 * diodes. The SHT4x runs from 1.08 V, so it wakes up and answers at 0x44 anyway.
 * A pull-down on the gated rail does not rescue it either: ~10 k still leaves the
 * part alive at ~1.8 V, and ~1 k kills the part but clamps SDA near 1.1 V, below
 * the 2.31 V VIH. What has to be switched is the BUS, not the power.
 *
 * That is exactly what this part does: one channel connected at a time, so only
 * one 0x44 is ever visible to the master. It costs no MCU pins (it is itself an
 * I2C device) and no extra signals across the board-to-board connector.
 *
 * Topology (see docs/hardware-interface.md):
 *   MCU I2C ---+--- TCA9548A (0x70) --+-- ch0 -- SHT45 #0 (0x44)
 *              |                      +-- ch1 -- SHT45 #1 (0x44)
 *              |                      +-- ch2 -- SHT45 #2 (0x44)
 *              +--- SCD41 (0x62)   <-- upstream: no collision, no channel needed
 *
 * The device has no register map. You write ONE byte, a bitmask of the channels
 * to connect; 0x00 disconnects everything. We only ever set a single bit, so the
 * three sensors are never on the bus together.
 */
#ifndef TCA9548A_H
#define TCA9548A_H

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>

/* Base address; 0x70..0x77 depending on the A2/A1/A0 strapping pins. */
#define TCA9548A_I2C_ADDR   0x70

/* Connect exactly one downstream channel (0..7). Returns 1 on ACK, else 0. */
int tca9548a_select(TwoWire *w, uint8_t addr, uint8_t channel);

/* Disconnect every channel. Do this before talking to an UPSTREAM device, and
 * before sleep, so nothing downstream is left bridged onto the bus.
 * Returns 1 on ACK, else 0. */
int tca9548a_none(TwoWire *w, uint8_t addr);

#endif /* TCA9548A_H */
