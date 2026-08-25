/* board.h — clock config, microsecond delay (DWT), fault handler. */
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void SystemClock_Config(void);   /* MSI 16 MHz sysclk, HSI16 for LPUART, LSE for RTC */
void DWT_Delay_Init(void);       /* enable the cycle counter used by delay_us()      */
void delay_us(uint32_t us);      /* busy-wait, cycle-accurate — used by 1-Wire        */
void Error_Handler(void);

#endif /* BOARD_H */
