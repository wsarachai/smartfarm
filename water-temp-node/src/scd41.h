/*
 * scd41.h — Sensirion SCD41 CO2 (photoacoustic NDIR) + temperature + humidity
 * over I2C. Hand-rolled on the shared sensirion_i2c helper, no vendor library.
 *
 * Address 0x62, shares the bus (and pull-ups) with the SHT45.
 *
 * TWO measurement modes, because the two hosts have opposite constraints:
 *
 *   PERIODIC (scd41_start_periodic + scd41_data_ready + scd41_read)
 *       The sensor free-runs, producing a sample every 5 s. Right for the
 *       mains/USB-powered F103 prototype, which is awake anyway. Costs ~15 mA
 *       average forever, so it is wrong for a battery node.
 *
 *   SINGLE SHOT (scd41_measure_single_shot)
 *       One on-demand measurement, 5 s, then idle. The datasheet quotes the cost
 *       duty-averaged: 0.45 mA typ / 0.5 mA max at 3.3 V for one shot every five
 *       minutes, i.e. ~135 mAs of charge per shot, drawn as 175 mA typ / 205 mA
 *       max bursts. Right for the power-gated WL55 node, which wakes every
 *       15 minutes. This command is SCD41-only (the SCD40 does not have it).
 *
 * >>> The single-shot accuracy caveat, which matters for the battery node <<<
 * The datasheet is explicit that the FIRST single-shot reading after power-up
 * is not trustworthy — the photoacoustic cell needs a measurement cycle to
 * settle. Take a throwaway reading first (scd41_measure_single_shot() twice,
 * or pass warmup=1 to scd41_read_single_shot()) and use the second. That is
 * ~10 s of sensor-on time per wake, which IS the CO2 sensor's whole energy cost
 * — 2 shots x 96 wakes x 135 mAs = ~0.024 Wh/day. See README, "CO2 on a solar
 * node".
 *
 * The SCD41 also reports its own temperature and humidity. We deliberately do
 * NOT use them for telemetry: the part self-heats, so its T/RH read high, and
 * the SHT45 next to it is both more accurate and unheated. They are exposed
 * only because scd41_read() gets them for free and they are useful in bring-up.
 */
#ifndef SCD41_H
#define SCD41_H

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>

#define SCD41_I2C_ADDR            0x62

/* Blocking durations the datasheet specifies for the commands we issue (ms). */
#define SCD41_SINGLE_SHOT_MS      5000   /* measure_single_shot execution time */
#define SCD41_STOP_PERIODIC_MS     500   /* stop_periodic_measurement          */
#define SCD41_POWER_UP_MS           30   /* VDD applied -> ready for commands   */
#define SCD41_PERIODIC_INTERVAL_MS 5000  /* periodic-mode sample interval       */
#define SCD41_PERSIST_MS           800   /* persist_settings — writes EEPROM    */
#define SCD41_FRC_MS               400   /* perform_forced_recalibration        */
#define SCD41_CMD_MS                 2   /* the 1 ms short commands, +1 margin  */
#define SCD41_FRC_FAILED        0xFFFF   /* FRC return value meaning "no"       */

/*
 * Every duration above is the datasheet's own maximum command time (SCD4x v1.7,
 * April 2025 — Table 7 for power-up, Table 9 for the rest). The one exception is
 * SCD41_CMD_MS: the datasheet gives 1 ms for the short read commands and this
 * allows 2, because an early read is a NACK rather than a retry and the extra
 * millisecond is free on a node that wakes every fifteen minutes.
 *
 * SCD41_POWER_UP_MS was 1000 ms until 2026-09. That number pre-dated this
 * datasheet revision and nothing in v1.7 supports it — Table 7 says "Power-up
 * time, after hard reset, VDD >= 2.25 V: max 30 ms". It made no difference here
 * (the gated rail has been up for the whole 750 ms DS18B20 conversion plus three
 * SHT45 reads before the SCD41 is addressed), but it is wrong by a factor of
 * thirty and would matter in any design where it sits on the critical path.
 */

/*
 * Probe the sensor on bus `w` and remember it for later calls.
 *
 * Stops any periodic measurement first — the SCD4x rejects most commands while
 * measuring, and after an MCU-only reset (or a crash) the sensor can still be
 * free-running from the previous session. Then reads the serial number as the
 * proof-of-life check. Returns 1 if a working SCD41 answered, else 0.
 *
 * The caller must already have waited SCD41_POWER_UP_MS after applying VDD.
 */
int scd41_init(TwoWire *w, uint8_t addr);

/* Start free-running 5 s measurements. Returns 1 on ACK. */
int scd41_start_periodic(void);

/* Stop free-running measurements. Blocks SCD41_STOP_PERIODIC_MS. Returns 1 on ACK. */
int scd41_stop_periodic(void);

/*
 * Is a periodic sample waiting? Writes 1/0 to *ready. Returns 1 if the query
 * itself succeeded (so a false return is a bus fault, not "no data yet").
 */
int scd41_data_ready(int *ready);

/*
 * Read the most recent measurement. co2 in ppm; temperature in centi-degC;
 * humidity in %RH x100. Any out pointer may be NULL. Returns 1 on success.
 *
 * In periodic mode call this only when scd41_data_ready() says so — reading
 * early returns the previous sample again.
 */
int scd41_read(uint16_t *out_co2_ppm, int16_t *out_t_c100, uint16_t *out_rh_x100);

/*
 * One on-demand measurement (SCD41 only). BLOCKS ~5 s per shot.
 *
 * With warmup != 0 it takes a throwaway measurement first and returns the
 * second — ~10 s total, but the only reading worth trusting from a sensor that
 * was powered off (see the header comment above).
 *
 * Returns 1 on success.
 */
int scd41_read_single_shot(int warmup, uint16_t *out_co2_ppm,
                           int16_t *out_t_c100, uint16_t *out_rh_x100);

/*
 * Feed the sensor the current ambient pressure (hPa) for CO2 density
 * compensation. Worth ~1 % of reading per 10 hPa, so it is a real correction
 * where a BME280 is fitted (the F103 prototype). Returns 1 on ACK.
 *
 * Accepted in periodic mode as well as idle, unlike most SCD4x settings.
 */
int scd41_set_ambient_pressure(uint16_t hpa);

/* Read the 48-bit serial number — bring-up proof of life. */
int scd41_serial(uint64_t *out_serial);

/*
 * ASC (Automatic Self-Calibration) — READ THIS BEFORE ENABLING IT.
 *
 * ASC assumes the sensor is "exposed to outdoor fresh air at 400 ppm at least
 * once for >3 minutes after every week of operation" (datasheet §3.8) and drags
 * the calibration toward its ASC baseline target (factory default 400 ppm, and
 * this node never writes it) using the lowest reading it has seen. In a MUSHROOM HOUSE that assumption is false in the worst possible
 * direction: the room never reaches 400 ppm, so ASC would keep pulling the
 * readings DOWN, the controller would think the air is fine, and it would
 * under-ventilate — which is exactly the failure the sensor is fitted to
 * prevent, and it looks completely normal on the dashboard.
 *
 * So this node runs with ASC OFF and recalibrates by hand with FRC, in outdoor
 * air, at each crop changeover. See docs/hardware-interface.md §3.
 *
 * ASC is ENABLED at the factory, so unlike everything else here this is a
 * setting the firmware must actively change, not merely verify.
 *
 * Reads the current state; writes 1/0 to *enabled. Returns 1 on success.
 */
int scd41_get_asc(int *enabled);

/*
 * Put ASC into the requested state and persist it to EEPROM.
 *
 * Reads first and writes ONLY when the state differs, because persisting costs
 * an EEPROM cycle. Safe (and cheap — one 1 ms read) to call on every wake; that
 * is deliberate, so a replaced sensor is corrected automatically instead of
 * silently running with the factory default. Returns 1 if the sensor now holds
 * the requested state.
 */
int scd41_ensure_asc(int want_enabled);

/*
 * Altitude compensation, the persistent counterpart of
 * scd41_set_ambient_pressure().
 *
 * set_ambient_pressure lives in RAM and is lost every time the gated rail drops
 * — useless on a node that power-cycles the sensor every 15 minutes. The
 * sensor's altitude setting is EEPROM-backed instead, so it is written once and
 * survives. Same read-before-write rule as ASC. Returns 1 if the sensor now
 * holds `metres`.
 */
int scd41_ensure_altitude(uint16_t metres);

/*
 * Forced recalibration to a known CO2 concentration — the SERVICE operation
 * that replaces ASC on this node.
 *
 * The datasheet's preconditions are strict and this call does NOT enforce them:
 * the sensor must already have been running for >3 minutes in homogeneous,
 * constant CO2 (in single-shot mode that means >3 shots at 1-minute intervals)
 * at its normal supply voltage, with altitude/pressure compensation already
 * set. Take the head outdoors, let it run, then call this with the reference
 * concentration (~420 ppm for outdoor air).
 *
 * Writes the applied correction in ppm to *out_correction when non-NULL.
 * Returns 1 on success, 0 if the sensor reported SCD41_FRC_FAILED — which means
 * it had not been measuring long enough, not that the sensor is broken.
 */
int scd41_perform_frc(uint16_t target_ppm, int16_t *out_correction);

#endif /* SCD41_H */
