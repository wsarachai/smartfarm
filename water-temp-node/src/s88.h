/*
 * s88.h — Senseair S88 (LP / GH) CO2 sensor, Modbus RTU over RS-485.
 *
 * REPLACED THE SCD41 (2026-08). Differences that matter to the caller:
 *
 *   - It is NOT an I2C device. It hangs off LPUART1 (PC1/PC0) through a 3.3 V
 *     RS-485 transceiver whose DE+!RE are driven by S88_DE_PIN (PA7); the sensor
 *     end drives its own transceiver from the S88's UART_R/T pin.
 *   - It is NOT on the gated VSENS rail. It runs continuously from its own 5 V
 *     module output: the S88 has no sleep mode, measures every 4 s (LP) and its
 *     IIR filter and ABC both assume continuous operation. So there is no
 *     power-up settle, no warm-up shot to discard, and no pacing — just read
 *     the latest value each wake.
 *   - Modbus RTU frames carry a CRC16. A corrupted reply is DETECTABLE, unlike a
 *     corrupted I2C transaction, so s88_read_co2() retries rather than handing
 *     back a plausible wrong number.
 *
 * Register map: TDE14367 "Modbus on Senseair S88" rev 5 (2024-09-04).
 *   CO2 is INPUT register IR4 (address 0x0003), read with function 0x04 — NOT a
 *   holding register. IR1 (address 0x0000) is MeterStatus; its bits are checked
 *   before a CO2 value is accepted. The sensor answers with 2 stop bits; the
 *   8N1 receiver treats the second as idle, which is fine.
 */
#ifndef S88_H
#define S88_H

#include <Arduino.h>

/* IR1 MeterStatus bits (TDE14367 Table 2 / PSP14281 Table 7). */
#define S88_ST_FATAL        0x0001
#define S88_ST_ALGORITHM    0x0004
#define S88_ST_OUTPUT       0x0008
#define S88_ST_SELFDIAG     0x0010
#define S88_ST_OUT_OF_RANGE 0x0020
#define S88_ST_MEMORY       0x0040
#define S88_ST_WARM_UP      0x0080
/* Any of these means the CO2 value in the same frame is not trustworthy. */
#define S88_ST_BAD (S88_ST_FATAL | S88_ST_ALGORITHM | S88_ST_SELFDIAG | \
                    S88_ST_OUT_OF_RANGE | S88_ST_MEMORY | S88_ST_WARM_UP)

/* Bring up the UART and the DE pin. Safe to call more than once. */
void s88_begin(void);

/* Read the CO2 concentration in ppm (IR4), gated on MeterStatus (IR1).
 * Returns 1 and writes *out_ppm on success; 0 on timeout, CRC failure, Modbus
 * exception, or a status word with any S88_ST_BAD bit set. If out_status is
 * non-NULL the raw IR1 word is written there whenever a valid frame arrived,
 * even when the function returns 0 — that is how the caller tells "sensor
 * warming up" from "sensor not answering". */
int s88_read_co2(uint16_t *out_ppm, uint16_t *out_status);

/* Pressure compensation. The S88 has no barometer; it reads 1.6 % high or low
 * per kPa of deviation from 1013.25 hPa. HR27 "Default pressure" (LSB 0.1 hPa,
 * EEPROM) is loaded into HR4 at every power-up and makes IR4 pressure-
 * compensated permanently. This reads HR27 and, only if it differs from
 * dhpa, writes HR27 (permanent) and HR4 (effective now). EEPROM is rated
 * 10 000 writes, so the read-before-write matters. dhpa == 0 disables
 * compensation (the factory default); pass S88_SITE_PRESSURE_DHPA.
 * Returns 1 if the sensor now holds dhpa, 0 on communication failure. */
int s88_apply_site_pressure(int16_t dhpa);

#endif /* S88_H */
