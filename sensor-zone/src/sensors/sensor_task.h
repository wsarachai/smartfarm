#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"

/**
 * @brief Starts the sensor FreeRTOS task.
 *        The task reads DHT22 (temperature + humidity) and soil moisture ADC
 *        every SENSOR_POLL_INTERVAL_MS milliseconds, then POSTs the readings
 *        as JSON to the web-server via http_client_post_sensor_data().
 *        Safe to call multiple times — subsequent calls are no-ops.
 */
esp_err_t sensor_task_start(void);

#endif // SENSOR_TASK_H
