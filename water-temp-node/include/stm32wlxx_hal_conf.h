/*
 * stm32wlxx_hal_conf.h — HAL module configuration for water-temp-node.
 * Trimmed from ST's template to the modules this firmware actually uses. The
 * important line for LoRa is HAL_SUBGHZ_MODULE_ENABLED (off in ST's default!).
 * PlatformIO's project include/ is searched before the framework, so this file
 * overrides any default that ships with framework-stm32cubewl.
 */
#ifndef STM32WLxx_HAL_CONF_H
#define STM32WLxx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SUBGHZ_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED

/* ########################## Oscillator Values ############################ */
#if !defined  (HSE_VALUE)
#define HSE_VALUE               32000000UL   /* radio TCXO, 32 MHz            */
#endif
#if !defined  (HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT     100U
#endif
#if !defined  (MSI_VALUE)
#define MSI_VALUE               4000000UL
#endif
#if !defined  (HSI_VALUE)
#define HSI_VALUE               16000000UL
#endif
#if !defined  (LSI_VALUE)
#define LSI_VALUE               32000UL
#endif
#if !defined  (LSE_VALUE)
#define LSE_VALUE               32768UL      /* on-board 32.768 kHz crystal   */
#endif
#if !defined  (LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT     5000U
#endif
#if !defined  (VDD_VALUE)
#define VDD_VALUE               3300UL
#endif

/* ########################## System Configuration ######################## */
#define TICK_INT_PRIORITY       15U
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE       1U

#define USE_HAL_ADC_REGISTER_CALLBACKS     0U
#define USE_HAL_RTC_REGISTER_CALLBACKS     0U
#define USE_HAL_SUBGHZ_REGISTER_CALLBACKS  0U
#define USE_HAL_UART_REGISTER_CALLBACKS    0U

#define USE_SPI_CRC             0U

/* ########################## Assert Selection ############################# */
/* #define USE_FULL_ASSERT      1U */

/* ################## Register callback feature guards #################### */

/* Includes ------------------------------------------------------------------*/
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32wlxx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32wlxx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32wlxx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32wlxx_hal_cortex.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32wlxx_hal_adc.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32wlxx_hal_exti.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32wlxx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32wlxx_hal_pwr.h"
#endif
#ifdef HAL_RTC_MODULE_ENABLED
#include "stm32wlxx_hal_rtc.h"
#endif
#ifdef HAL_SUBGHZ_MODULE_ENABLED
#include "stm32wlxx_hal_subghz.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32wlxx_hal_uart.h"
#endif

/* Assert ------------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32WLxx_HAL_CONF_H */
