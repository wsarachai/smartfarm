/*
 * hal_conf_extra.h — STM32duino picks this file up (if present on the include
 * path) and includes it at the end of its generated stm32wlxx_hal_conf.h, so we
 * can enable HAL modules the core leaves off by default.
 *
 * The one we need: HAL_SUBGHZ — the sub-GHz radio driver our LoRa code
 * (src/lora/subghz_lora.c) is built on. Without this the HAL's
 * stm32wlxx_hal_subghz.c is #ifdef'd out and every HAL_SUBGHZ_* symbol is an
 * undefined reference at link time.
 */
#ifndef HAL_CONF_EXTRA_H
#define HAL_CONF_EXTRA_H

#ifndef HAL_SUBGHZ_MODULE_ENABLED
#define HAL_SUBGHZ_MODULE_ENABLED
#endif

/* Needed by the node's raw-HAL deep-sleep (Stop2) + RTC wake path in main.cpp.
 * Harmless on the gateway (it just doesn't use them). */
#ifndef HAL_RTC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#endif
#ifndef HAL_PWR_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#endif

#endif /* HAL_CONF_EXTRA_H */
