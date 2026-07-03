#include <stdio.h>
#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rgb_led.h"

static const char *TAG = "rgb_led";
static bool s_wifi_connected = false;

static uint32_t channel_duty_from_8bit(uint8_t value)
{
#if RGB_LED_ACTIVE_LOW
  return 255 - value;
#else
  return value;
#endif
}

void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
  ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_0, channel_duty_from_8bit(r));
  ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_0);

  ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_1, channel_duty_from_8bit(g));
  ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_1);

  ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_2, channel_duty_from_8bit(b));
  ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_2);
}

void rgb_led_set_wifi_connected(bool connected)
{
  s_wifi_connected = connected;
  if (connected)
  {
    ESP_LOGI(TAG, "WiFi connected - LED set to GREEN");
    rgb_set(RGB_COLOR_GREEN);
  }
  else
  {
    ESP_LOGI(TAG, "WiFi disconnected - LED set to RED");
    rgb_set(RGB_COLOR_RED);
  }
}

void rgb_led_flash_success(void)
{
  ESP_LOGI(TAG, "Data sent successfully - flashing LED");
  rgb_set(0, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(200));
  rgb_set(RGB_COLOR_ORANGE);
  vTaskDelay(pdMS_TO_TICKS(200));
  rgb_set(0, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(200));
  rgb_set(RGB_COLOR_ORANGE);
  vTaskDelay(pdMS_TO_TICKS(200));
  rgb_set(0, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(200));
  rgb_set(RGB_COLOR_ORANGE);
  ESP_LOGI(TAG, "Flash complete - restoring LED state");
  if (s_wifi_connected)
  {
    rgb_set(RGB_COLOR_GREEN);
  }
  else
  {
    rgb_set(RGB_COLOR_RED);
  }
}

esp_err_t rgb_led_init(void)
{
  ledc_timer_config_t timer_cfg = {
      .speed_mode = RGB_LEDC_MODE,
      .duty_resolution = RGB_LEDC_RESOLUTION,
      .timer_num = RGB_LEDC_TIMER,
      .freq_hz = RGB_LEDC_FREQUENCY_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

  ledc_channel_config_t red_cfg = {
      .gpio_num = RGB_LED_GPIO_R,
      .speed_mode = RGB_LEDC_MODE,
      .channel = LEDC_CHANNEL_0,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = RGB_LEDC_TIMER,
      .duty = channel_duty_from_8bit(0),
      .hpoint = 0,
  };

  ledc_channel_config_t green_cfg = {
      .gpio_num = RGB_LED_GPIO_G,
      .speed_mode = RGB_LEDC_MODE,
      .channel = LEDC_CHANNEL_1,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = RGB_LEDC_TIMER,
      .duty = channel_duty_from_8bit(0),
      .hpoint = 0,
  };

  ledc_channel_config_t blue_cfg = {
      .gpio_num = RGB_LED_GPIO_B,
      .speed_mode = RGB_LEDC_MODE,
      .channel = LEDC_CHANNEL_2,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = RGB_LEDC_TIMER,
      .duty = channel_duty_from_8bit(0),
      .hpoint = 0,
  };

  ESP_ERROR_CHECK(ledc_channel_config(&red_cfg));
  ESP_ERROR_CHECK(ledc_channel_config(&green_cfg));
  ESP_ERROR_CHECK(ledc_channel_config(&blue_cfg));

  rgb_set(0, 0, 0);
  s_wifi_connected = false;

  return ESP_OK;
}
