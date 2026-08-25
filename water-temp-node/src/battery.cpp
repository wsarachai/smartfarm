/* battery.cpp — supply-rail (== battery) voltage via the internal reference.
 * Uses STM32duino's analog API, which reads VREFINT and applies the factory
 * calibration to yield VDDA in millivolts — no manual ADC/clock setup needed.
 * Valid when the battery directly feeds the 3.0-3.6 V domain (2xAA / LiFePO4).
 */
#include "battery.h"
#include <Arduino.h>

uint16_t battery_read_mv(void)
{
#if defined(AVREF)
    analogReadResolution(12);
    int raw = analogRead(AVREF);          /* internal VREFINT channel */
    if (raw <= 0) {
        return 0;
    }
    /* STM32duino helper: convert the VREFINT reading to VDDA in mV. */
    return (uint16_t)__LL_ADC_CALC_VREFANALOG_VOLTAGE((uint32_t)raw, LL_ADC_RESOLUTION_12B);
#else
    return 0;   /* board has no AVREF pseudo-channel defined */
#endif
}
