#include "http_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "rgb_led.h"
#include "wifi_sta.h"

// Stringize WIFI_SERVER_PORT (from secrets.h) so the URL stays in sync with
// the configured port. STR_() forces macro expansion before stringization.
#define STR_(x) #x
#define STR(x) STR_(x)

#define SENSOR_UPDATE_PATH "/api/v1/telemetry"
#define SENSOR_UPDATE_URL "http://" WIFI_SERVER_HOST ":" STR(WIFI_SERVER_PORT) SENSOR_UPDATE_PATH

static const char *TAG = "http_client";

// Telemetry device_id comes from secrets.h (DEVICE_ID) so the dashboard shows a
// friendly name (e.g. "zone-a") instead of the raw MAC. Falls back to a generic
// id only if an older secrets.h predates this field.
#ifndef DEVICE_ID
#define DEVICE_ID "sensor-zone"
#endif

static const char *http_client_get_device_id(void)
{
    return DEVICE_ID;
}

esp_err_t http_client_post_sensor_data(float temperature, float humidity, float soil_moisture)
{
    const char *device_id = http_client_get_device_id();

    char body[192];
    int written = snprintf(body, sizeof(body),
                           "{\"device_id\":\"%s\",\"metrics\":{\"temperature\":%.2f,\"humidity\":%.2f,\"soil_moisture\":%.2f}}",
                           device_id,
                           temperature,
                           humidity,
                           soil_moisture);
    if (written < 0 || written >= (int)sizeof(body))
    {
        ESP_LOGE(TAG, "JSON body truncated");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = SENSOR_UPDATE_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set_header failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_http_client_set_post_field(client, body, (int)strlen(body));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set_post_field failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_http_client_set_header(client, "X-Device-Id", device_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set_header X-Device-Id failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300)
        {
            ESP_LOGI(TAG, "POST %s -> HTTP %d  body=%s", SENSOR_UPDATE_PATH, status, body);
            rgb_led_flash_success();
        }
        else
        {
            ESP_LOGW(TAG, "POST %s -> unexpected HTTP %d", SENSOR_UPDATE_PATH, status);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
    }

cleanup:
    esp_http_client_cleanup(client);
    return err;
}
