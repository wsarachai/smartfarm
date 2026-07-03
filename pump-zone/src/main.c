#include <stdbool.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "http_server.h"
#include "main.h"
#include "relay.h"
#include "rgb_led.h"
#include "task_settings.h"
#include "wifi_sta.h"

static const char TAG[] = "main_app";

// The "status" color reflects connectivity (BLUE boot, GREEN connected, RED
// disconnected). While the pump is running it is temporarily overridden with
// MAGENTA; when the pump stops the status color is restored.
static rgb_led_color_id_t s_status_led_color = RGB_LED_COLOR_BLUE;
static bool s_last_relay_state = false;

// Queue handle used to post events to the main task.
QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t event_id)
{
  app_event_t msg;
  msg.event_id = event_id;
  return xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
}

// Sets the connectivity status color. Takes effect immediately only when the
// pump is OFF; while the pump is ON, MAGENTA stays until it stops.
static void set_status_color(rgb_led_color_id_t color)
{
  s_status_led_color = color;
  if (!relay_get_state())
  {
    rgb_led_set_color_by_id(color);
  }
}

static void main_task(void *pvParameters)
{
  (void)pvParameters;

  app_queue_handle = xQueueCreate(10, sizeof(app_event_t));
  if (app_queue_handle == NULL)
  {
    ESP_LOGE(TAG, "Failed to create app queue");
    vTaskDelete(NULL);
  }

  esp_err_t led_init_status = rgb_led_init();
  if (led_init_status != ESP_OK)
  {
    ESP_LOGE(TAG, "RGB LED init failed: %s", esp_err_to_name(led_init_status));
  }

  relay_init();
  ESP_LOGI(TAG, "Pump node \"%s\" — relay initialized", DEVICE_ID);

  // BLUE while booting / connecting.
  set_status_color(RGB_LED_COLOR_BLUE);

  wifi_sta_start();

  while (1)
  {
    app_event_t app_event;
    if (xQueueReceive(app_queue_handle, &app_event, pdMS_TO_TICKS(1000)) == pdPASS)
    {
      switch (app_event.event_id)
      {
      case APP_MSG_WIFI_CONNECTED_GOT_IP:
        ESP_LOGI(TAG, "WiFi connected, got IP");
        // Bring up the relay control HTTP server (idempotent on reconnect).
        if (http_server_start() == ESP_OK)
        {
          set_status_color(RGB_LED_COLOR_GREEN);
        }
        else
        {
          set_status_color(RGB_LED_COLOR_RED);
        }
        break;

      case APP_MSG_WIFI_DISCONNECTED:
        ESP_LOGW(TAG, "WiFi disconnected");
        set_status_color(RGB_LED_COLOR_RED);
        break;

      default:
        break;
      }
    }

    // Reflect relay state on the LED: MAGENTA while the pump runs, restore the
    // connectivity status color when it stops.
    bool relay_on = relay_get_state();
    if (relay_on != s_last_relay_state)
    {
      s_last_relay_state = relay_on;
      if (relay_on)
      {
        ESP_LOGI(TAG, "Pump ON — LED MAGENTA");
        rgb_led_set_color_by_id(RGB_LED_COLOR_MAGENTA);
      }
      else
      {
        ESP_LOGI(TAG, "Pump OFF — restoring status LED");
        rgb_led_set_color_by_id(s_status_led_color);
      }
    }
  }
}

void app_main(void)
{
  // Suppress verbose per-pin config logs from the ESP-IDF GPIO driver.
  esp_log_level_set("gpio", ESP_LOG_WARN);

  xTaskCreatePinnedToCore(main_task,
                          "main_task",
                          MAIN_TASK_STACK_SIZE,
                          NULL,
                          MAIN_TASK_PRIORITY,
                          NULL,
                          MAIN_TASK_CORE_ID);
}
