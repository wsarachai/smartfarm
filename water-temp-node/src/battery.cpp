/* battery.cpp — 24 V bank voltage through a divider, scaled by the real VDDA.
 * See battery.h for why this is no longer a bare VREFINT read.
 */
#include "battery.h"
#include "node_config.h"
#include <Arduino.h>

uint16_t battery_read_mv(void)
{
#if defined(AVREF) && defined(VBAT_SENSE_PIN)
    analogReadResolution(12);

    /* 1. Recover the true VDDA. The buck is nominally 3.3 V but is allowed a few
     *    percent, and that error would otherwise land directly on the result. */
    int vref_raw = analogRead(AVREF);
    if (vref_raw <= 0) return 0;
    uint32_t vdda_mv =
        __LL_ADC_CALC_VREFANALOG_VOLTAGE((uint32_t)vref_raw, LL_ADC_RESOLUTION_12B);

    /* 2. Read the divider. Source impedance is R24||R25 = 27k, so the sampling
     *    time must be long; C10 across R25 is what makes this workable. */
    int div_raw = analogRead(VBAT_SENSE_PIN);
    if (div_raw < 0) return 0;

    /* 3. pin_mv = raw/4095 * vdda; bank_mv = pin_mv * (R24+R25)/R25.
     *    Done in 64-bit because 24000 mV * 4095 overflows 32 bits when scaled. */
    uint64_t num = (uint64_t)div_raw * vdda_mv
                 * (uint64_t)(VBAT_SENSE_NUM + VBAT_SENSE_DEN);
    uint64_t den = 4095ULL * (uint64_t)VBAT_SENSE_DEN;
    uint64_t mv  = num / den;

    if (mv > 65535ULL) mv = 65535ULL;   /* uint16 frame field */
    return (uint16_t)mv;
#else
    return 0;   /* no AVREF pseudo-channel, or no divider configured */
#endif
}
