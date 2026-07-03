#include <stdio.h>
#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"
#include "rgb_led.h"
#include "sensor_task.h"
#include "task_settings.h"
#include "wifi_sta.h"

static const char TAG[] = "main";

#define STATUS_LED_INACTIVE_LEVEL (!STATUS_LED_ACTIVE_LEVEL)
#define STATUS_LED_ENABLED (STATUS_LED_GPIO >= 0)

QueueHandle_t app_queue_handle;

static void status_led_set(bool on)
{
#if STATUS_LED_ENABLED
    gpio_set_level((gpio_num_t)STATUS_LED_GPIO,
                   on ? STATUS_LED_ACTIVE_LEVEL : STATUS_LED_INACTIVE_LEVEL);
#else
    (void)on;
#endif
}

static void status_led_init(void)
{
#if STATUS_LED_ENABLED
    ESP_LOGI(TAG,
             "Status LED config: gpio=%d active_level=%d blink_ms=%d",
             STATUS_LED_GPIO,
             STATUS_LED_ACTIVE_LEVEL,
             STATUS_LED_BLINK_INTERVAL_MS);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    status_led_set(false);
#else
    ESP_LOGW(TAG, "Status LED disabled (STATUS_LED_GPIO < 0)");
#endif
}

BaseType_t app_send_message(app_event_id_t event_id)
{
    app_event_t msg = {.event_id = event_id};
    return xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
}

static void main_task(void *pvParameters)
{
    (void)pvParameters;
    bool wifi_connected = false;
    bool led_on = false;
    TickType_t last_blink_tick = xTaskGetTickCount();

    app_queue_handle = xQueueCreate(10, sizeof(app_event_t));
    if (app_queue_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create app queue");
        vTaskDelete(NULL);
        return;
    }

    wifi_sta_start();
    sensor_task_start();
    status_led_init();

    while (1)
    {
        app_event_t event;
        if (xQueueReceive(app_queue_handle, &event, pdMS_TO_TICKS(100)) == pdPASS)
        {
            switch (event.event_id)
            {
            case APP_MSG_WIFI_CONNECTED_GOT_IP:
                ESP_LOGI(TAG, "APP_MSG_WIFI_CONNECTED_GOT_IP");
                wifi_connected = true;
                led_on = false;
                last_blink_tick = xTaskGetTickCount();
                status_led_set(led_on);
                rgb_led_set_wifi_connected(true);
                break;

            case APP_MSG_WIFI_DISCONNECTED:
                ESP_LOGW(TAG, "APP_MSG_WIFI_DISCONNECTED");
                wifi_connected = false;
                led_on = false;
                status_led_set(false);
                rgb_led_set_wifi_connected(false);
                break;

            default:
                break;
            }
        }

        if (wifi_connected)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_blink_tick) >= pdMS_TO_TICKS(STATUS_LED_BLINK_INTERVAL_MS))
            {
                led_on = !led_on;
                status_led_set(led_on);
                last_blink_tick = now;
            }
        }
    }
}

void app_main(void)
{
    // Suppress verbose per-pin config logs from the ESP-IDF GPIO driver.
    esp_log_level_set("gpio", ESP_LOG_WARN);

    rgb_led_init();

    xTaskCreatePinnedToCore(
        main_task,
        "main_task",
        MAIN_TASK_STACK_SIZE,
        NULL,
        MAIN_TASK_PRIORITY,
        NULL,
        MAIN_TASK_CORE_ID);
}
