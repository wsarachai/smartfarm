#include "sensor_task.h"

#include "dht22.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_client.h"
#include "soil_moisture_adc.h"
#include "task_settings.h"
#include "wifi_sta.h"

#define SENSOR_POLL_INTERVAL_MS 2000
// Force a WiFi reconnect after this many consecutive HTTP connection failures.
#define HTTP_CONNECT_FAIL_THRESHOLD 3

static const char *TAG = "sensor_task";

static bool s_started = false;
static int s_http_connect_fail_count = 0;

static void sensor_task_fn(void *pvParameters)
{
    (void)pvParameters;

    esp_err_t soil_err = soil_moisture_adc_init();
    if (soil_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Soil moisture ADC init failed: %s", esp_err_to_name(soil_err));
    }

    int reading = 0;

    while (1)
    {
        reading++;

        float humidity = 0.0f;
        float temperature = 0.0f;
        float soil_moisture = 0.0f;

        bool dht_ok = dht22_read(&humidity, &temperature);
        esp_err_t soil_ok = soil_moisture_adc_read_percent(&soil_moisture);

        if (!dht_ok)
        {
            ESP_LOGW(TAG, "[%d] DHT22 read failed — skipping POST", reading);
            vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
            continue;
        }

        if (soil_ok != ESP_OK)
        {
            ESP_LOGW(TAG, "[%d] Soil moisture read failed (%s) — using 0%%",
                     reading, esp_err_to_name(soil_ok));
            soil_moisture = 0.0f;
        }

        ESP_LOGI(TAG, "[%d] temp=%.1f°C  humidity=%.1f%%  soil=%.1f%%",
                 reading, temperature, humidity, soil_moisture);

        if (!wifi_sta_is_connected())
        {
            ESP_LOGW(TAG, "[%d] No network — skipping POST", reading);
            s_http_connect_fail_count = 0;
        }
        else
        {
            esp_err_t post_err = http_client_post_sensor_data(temperature, humidity, soil_moisture);
            if (post_err == ESP_OK)
            {
                s_http_connect_fail_count = 0;
            }
            else if (post_err != ESP_FAIL)
            {
                // ESP_FAIL means the server replied with a non-2xx status — it is reachable.
                // Any other error (ESP_ERR_HTTP_CONNECT, timeout, etc.) means the server
                // could not be reached at all, which usually means the AP rebooted and the
                // WiFi stack hasn't detected the loss yet.
                s_http_connect_fail_count++;
                ESP_LOGW(TAG, "[%d] HTTP connection error (%d/%d): %s",
                         reading, s_http_connect_fail_count, HTTP_CONNECT_FAIL_THRESHOLD,
                         esp_err_to_name(post_err));
                if (s_http_connect_fail_count >= HTTP_CONNECT_FAIL_THRESHOLD)
                {
                    s_http_connect_fail_count = 0;
                    wifi_sta_force_reconnect();
                }
            }
            else
            {
                ESP_LOGW(TAG, "[%d] POST rejected by server: %s",
                         reading, esp_err_to_name(post_err));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
    }
}

esp_err_t sensor_task_start(void)
{
    if (s_started)
    {
        return ESP_OK;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        sensor_task_fn,
        "sensor_task",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        NULL,
        SENSOR_TASK_CORE_ID);

    if (created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }

    s_started = true;
    ESP_LOGI(TAG, "Sensor task started");
    return ESP_OK;
}
