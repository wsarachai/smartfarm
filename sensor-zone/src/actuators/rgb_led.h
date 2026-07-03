#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define RGB_LED_GPIO_R 25
#define RGB_LED_GPIO_G 26
#define RGB_LED_GPIO_B 27

#define RGB_LED_ACTIVE_LOW 0

#define RGB_LEDC_MODE LEDC_LOW_SPEED_MODE
#define RGB_LEDC_TIMER LEDC_TIMER_0
#define RGB_LEDC_RESOLUTION LEDC_TIMER_8_BIT
#define RGB_LEDC_FREQUENCY_HZ 5000

#define RGB_COLOR_RED    0xFF, 0x00, 0x00
#define RGB_COLOR_GREEN  0x00, 0xFF, 0x00
#define RGB_COLOR_BLUE   0x00, 0x00, 0xFF
#define RGB_COLOR_ORANGE 0xFF, 0xA5, 0x00

typedef struct
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  const char *name;
} rgb_color_step_t;

esp_err_t rgb_led_init(void);
void rgb_set(uint8_t r, uint8_t g, uint8_t b);
void rgb_led_set_wifi_connected(bool connected);
void rgb_led_flash_success(void);

#endif // RGB_LED_H
