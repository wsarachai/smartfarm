#ifndef RGB_LED_H
#define RGB_LED_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

// GPIO pins (RGB_LED_GPIO_R/G/B) and polarity (RGB_LED_ACTIVE_LOW) come from the
// committed hardware config header so all wiring lives in one place.
#include "pump_config.h"

// LEDC (PWM) driver settings for the RGB LED — internal to this module.
#define RGB_LEDC_MODE LEDC_LOW_SPEED_MODE
#define RGB_LEDC_TIMER LEDC_TIMER_0
#define RGB_LEDC_RESOLUTION LEDC_TIMER_8_BIT
#define RGB_LEDC_FREQUENCY_HZ 5000

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    const char *name;
} rgb_led_color_step_t;

typedef enum
{
    RGB_LED_COLOR_RED = 0,
    RGB_LED_COLOR_GREEN,
    RGB_LED_COLOR_BLUE,
    RGB_LED_COLOR_YELLOW,
    RGB_LED_COLOR_CYAN,
    RGB_LED_COLOR_MAGENTA,
    RGB_LED_COLOR_WHITE,
    RGB_LED_COLOR_OFF,
    RGB_LED_COLOR_COUNT,
} rgb_led_color_id_t;

esp_err_t rgb_led_init(void);
esp_err_t rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b);
esp_err_t rgb_led_set_off(void);
esp_err_t rgb_led_set_color_by_id(rgb_led_color_id_t color_id);

#endif // RGB_LED_H
