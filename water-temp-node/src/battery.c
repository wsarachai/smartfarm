/* battery.c — see battery.h. VDDA = 3000mV * VREFINT_CAL / VREFINT_measured. */
#include "battery.h"
#include "board.h"
#include "stm32wlxx_hal.h"

/* STM32WL factory VREFINT calibration (measured at VDDA = 3.0 V), 12-bit. */
#define VREFINT_CAL_ADDR    ((uint16_t *)0x1FFF75AAU)
#define VREFINT_CAL_VREF_MV 3000U

uint16_t battery_read_mv(void)
{
    ADC_HandleTypeDef hadc = {0};
    __HAL_RCC_ADC_CLK_ENABLE();

    hadc.Instance                   = ADC;
    hadc.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV4;
    hadc.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc.Init.LowPowerAutoWait      = DISABLE;
    hadc.Init.ContinuousConvMode    = DISABLE;
    hadc.Init.NbrOfConversion       = 1;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc.Init.SamplingTimeCommon1   = ADC_SAMPLETIME_160CYCLES_5;
    hadc.Init.SamplingTimeCommon2   = ADC_SAMPLETIME_160CYCLES_5;

    if (HAL_ADC_Init(&hadc) != HAL_OK) return 0;

    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = ADC_CHANNEL_VREFINT;
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc, &ch) != HAL_OK) { HAL_ADC_DeInit(&hadc); return 0; }

    if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK) { HAL_ADC_DeInit(&hadc); return 0; }

    /* VREFINT needs a moment to stabilize after the buffer is enabled. */
    delay_us(20);

    uint16_t vref_meas = 0;
    if (HAL_ADC_Start(&hadc) == HAL_OK &&
        HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
        vref_meas = (uint16_t)HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);
    HAL_ADC_DeInit(&hadc);

    if (vref_meas == 0) return 0;
    uint16_t cal = *VREFINT_CAL_ADDR;
    return (uint16_t)(((uint32_t)VREFINT_CAL_VREF_MV * cal) / vref_meas);
}
