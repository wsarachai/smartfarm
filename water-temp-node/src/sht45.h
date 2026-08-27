/*
 * sht45.h — Sensirion SHT45 temperature + humidity over I2C.
 *
 * Hand-rolled on the shared sensirion_i2c helper (no Sensirion library), the way
 * ds18b20.cpp and sx1278.cpp are done here — the WL55 env carries no lib_deps.
 *
 * The part is trivial to drive: there is no configuration and no state machine.
 * One command byte kicks a conversion, and ~10 ms later six bytes come back
 * (T word + CRC, RH word + CRC). Nothing to keep alive between reads, which is
 * exactly what the power-gated WL55 node wants.
 *
 * Wiring: 3V3, GND, SDA, SCL, with 4.7 k pull-ups on both lines (the SCD41
 * shares the same bus and the same pull-ups). Address 0x44 by default.
 *
 * Accuracy: +-1.0 %RH / +-0.1 degC — better than the BME280 on both counts,
 * which is why the frame takes air temp + humidity from here when it is fitted
 * (see LORA_FLAG_SHT in lora/lora_packet.h).
 */
#ifndef SHT45_H
#define SHT45_H

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>

#define SHT45_I2C_ADDR      0x44   /* 0x45 on the -B variant / ADDR pin high */

/* Worst-case duration of a high-precision measurement (ms), per the datasheet. */
#define SHT45_MEASURE_MS    10

/*
 * Probe the sensor on bus `w` and remember it for later calls. Issues a soft
 * reset, then a throwaway high-precision read to confirm the part answers with
 * valid CRCs. Returns 1 if a working SHT45 is present, else 0.
 */
int sht45_init(TwoWire *w, uint8_t addr);

/*
 * High-precision measurement. On success writes centi-degC to *out_t_c100
 * (e.g. 24.13 C -> 2413) and %RH x100 to *out_rh_x100, and returns 1.
 * Returns 0 on NAK or CRC failure. Blocks SHT45_MEASURE_MS.
 *
 * Humidity is clamped to 0..100 %RH: the raw transfer function is defined
 * slightly outside that range on purpose (so you can see a saturated sensor),
 * but a >100 % reading in telemetry is just confusing.
 */
int sht45_read(int16_t *out_t_c100, uint16_t *out_rh_x100);

/* Read the 32-bit serial number — a cheap "is it really there" bring-up check. */
int sht45_serial(uint32_t *out_serial);

#endif /* SHT45_H */
