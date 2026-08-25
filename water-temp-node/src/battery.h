/*
 * battery.h — supply-rail (== battery) voltage via the internal VREFINT and the
 * factory calibration, with zero external parts. Valid when the battery directly
 * feeds the 3.0-3.6 V domain (2xAA / LiFePO4). Returns millivolts, 0 on failure.
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

uint16_t battery_read_mv(void);

#endif /* BATTERY_H */
