#ifndef TASK_SETTINGS_H
#define TASK_SETTINGS_H

// Shared FreeRTOS task defaults for this component.
#define TASK_STACK_SIZE_DEFAULT 4096
#define TASK_PRIORITY_DEFAULT 5
#define TASK_CORE_ID_DEFAULT 0

// Main application task
#define MAIN_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define MAIN_TASK_PRIORITY TASK_PRIORITY_DEFAULT
#define MAIN_TASK_CORE_ID TASK_CORE_ID_DEFAULT

// WiFi STA task (init + event task, self-deletes after setup)
#define WIFI_STA_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define WIFI_STA_TASK_PRIORITY TASK_PRIORITY_DEFAULT
#define WIFI_STA_TASK_CORE_ID TASK_CORE_ID_DEFAULT

// Sensor read + HTTP POST task
#define SENSOR_TASK_STACK_SIZE (TASK_STACK_SIZE_DEFAULT * 2)
#define SENSOR_TASK_PRIORITY (TASK_PRIORITY_DEFAULT + 3)
#define SENSOR_TASK_CORE_ID 1

// Status LED behavior (change these for your board wiring)
// ESP32-WROOM-32U dev boards usually expose onboard LED on GPIO2 and often active-low.
// Set STATUS_LED_GPIO to -1 if your board has no onboard LED.
#define STATUS_LED_GPIO 2
#define STATUS_LED_ACTIVE_LEVEL 0
#define STATUS_LED_BLINK_INTERVAL_MS 500

#endif // TASK_SETTINGS_H
