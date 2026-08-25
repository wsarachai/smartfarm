/* board.c — see board.h. */
#include "board.h"
#include "stm32wlxx_hal.h"

/*
 * Clock tree:
 *   SYSCLK = MSI @ 16 MHz   (low-power friendly, plenty for bit-bang + radio SPI)
 *   HSI16  = on             (kernel clock for LPUART1 so 115200 is accurate)
 *   LSE    = on             (32.768 kHz -> RTC, for the Stop2 wake timer)
 * The radio runs off its own 32 MHz TCXO (DIO3-powered) independent of SYSCLK.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    HAL_PWR_EnableBkUpAccess();               /* needed to configure LSE */
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_HSI |
                         RCC_OSCILLATORTYPE_LSE;
    osc.MSIState = RCC_MSI_ON;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.MSIClockRange = RCC_MSIRANGE_8;       /* 16 MHz */
    osc.HSIState = RCC_HSI_ON;
    osc.LSEState = RCC_LSE_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                    RCC_CLOCKTYPE_HCLK3;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.AHBCLK3Divider = RCC_SYSCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }

    /* LPUART1 kernel clock <- HSI16; RTC clock <- LSE. */
    RCC_PeriphCLKInitTypeDef pclk = {0};
    pclk.PeriphClockSelection = RCC_PERIPHCLK_LPUART1 | RCC_PERIPHCLK_RTC |
                                RCC_PERIPHCLK_ADC;
    pclk.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_HSI;
    pclk.RTCClockSelection     = RCC_RTCCLKSOURCE_LSE;
    pclk.AdcClockSelection     = RCC_ADCCLKSOURCE_HSI;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
        Error_Handler();
    }
}

/* ---- DWT cycle-counter microsecond delay -------------------------------- */
void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {
        /* busy wait */
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* hang — a watchdog reset would be the field recovery */
    }
}
