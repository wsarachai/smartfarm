/*
 * battery.h — 24 V solar bank voltage, via an external divider.
 *
 * CHANGED 2026-08. This used to read VREFINT alone and return VDDA, which was
 * valid only while the battery WAS the 3.0-3.6 V domain. The supply is now a
 * 24 V bank behind a charge controller, and the MCU sits behind a buck — so
 * VDDA is a constant 3.3 V and says nothing about the bank.
 *
 * It now reads BOTH:
 *   - VREFINT, to recover the ACTUAL VDDA (the buck's tolerance would otherwise
 *     appear as error on every reported bank voltage), and
 *   - VBAT_SENSE_PIN, the 300k/30k divider off the 24 V input.
 *
 * Returns bank millivolts (e.g. 24000 for 24.0 V), 0 on failure.
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

uint16_t battery_read_mv(void);

#endif /* BATTERY_H */
