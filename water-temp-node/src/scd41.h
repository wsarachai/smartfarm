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
 *       One on-demand measurement, ~5 s of ~50 mA average with ~205 mA peaks,
 *       then idle. Right for the power-gated WL55 node, which wakes every
 *       15 minutes. This command is SCD41-only (the SCD40 does not have it).
 *
 * >>> The single-shot accuracy caveat, which matters for the battery node <<<
 * The datasheet is explicit that the FIRST single-shot reading after power-up
 * is not trustworthy — the photoacoustic cell needs a measurement cycle to
 * settle. Take a throwaway reading first (scd41_measure_single_shot() twice,
 * or pass warmup=1 to scd41_read_single_shot()) and use the second. That is
 * ~10 s of sensor-on time per wake, which IS the CO2 sensor's whole battery
 * cost — see README, "CO2 on a battery node".
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
#define SCD41_POWER_UP_MS         1000   /* after VDD is applied, before cmds   */
#define SCD41_PERIODIC_INTERVAL_MS 5000  /* periodic-mode sample interval       */

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

#endif /* SCD41_H */
