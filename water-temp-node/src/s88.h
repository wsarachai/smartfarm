/*
 * s88.h — Senseair S88 LP CO2 sensor, Modbus RTU over RS-485.
 *
 * REPLACED THE SCD41 (2026-08). Differences that matter to the caller:
 *
 *   - It is NOT an I2C device. It hangs off LPUART1 (PC1/PC0) through an
 *     auto-direction RS-485 transceiver; the sensor end drives its own
 *     transceiver from the S88's UART_R/T pin, so there is no DE signal here.
 *   - It is NOT on the gated VSENS rail. It runs continuously from its own 5.1 V
 *     buck output, because its ABC (Automatic Baseline Correction) has an 8-day
 *     period and the datasheet specifies accuracy at continuous operation. So
 *     there is no power-up settle, no warm-up shot to discard, and no pacing —
 *     just read the latest value each wake.
 *   - Modbus RTU frames carry a CRC16. A corrupted reply is DETECTABLE, unlike a
 *     corrupted I2C transaction, so s88_read_co2() retries rather than handing
 *     back a plausible wrong number.
 *
 * UNVERIFIED: S88_MODBUS_ADDR and S88_CO2_REG in node_config.h are placeholders.
 * The register map is not in the product spec (PSP14281); it is in TDE14367,
 * "Modbus on Senseair S88". Check both before trusting a reading.
 */
#ifndef S88_H
#define S88_H

#include <Arduino.h>

/* Bring up the UART. Safe to call more than once. */
void s88_begin(void);

/* Read the CO2 concentration in ppm.
 * Returns 1 and writes *out_ppm on success, 0 on timeout/CRC failure. */
int s88_read_co2(uint16_t *out_ppm);

#endif /* S88_H */
