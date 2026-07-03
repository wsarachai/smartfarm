#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"

/**
 * @brief POSTs sensor readings as JSON to the web-server.
 *
 * Target URL: http://WIFI_SERVER_HOST:WIFI_SERVER_PORT/api/v1/telemetry
 * Body: {"device_id":"<unique-id>","metrics":{"temperature":<val>,"humidity":<val>,"soil_moisture":<val>}}
 * Header: X-Device-Id: <unique-id>
 *
 * @return ESP_OK on HTTP 2xx response, ESP_FAIL otherwise.
 */
esp_err_t http_client_post_sensor_data(float temperature, float humidity, float soil_moisture);

#endif // HTTP_CLIENT_H
