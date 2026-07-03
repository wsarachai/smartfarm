#ifndef TASK_SETTINGS_H
#define TASK_SETTINGS_H

// Shared FreeRTOS task defaults for this component.
#define TASK_STACK_SIZE_DEFAULT 4096
#define TASK_PRIORITY_DEFAULT 5
#define TASK_CORE_ID_DEFAULT 0

// Main application task (event loop + relay->LED status).
#define MAIN_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define MAIN_TASK_PRIORITY TASK_PRIORITY_DEFAULT
#define MAIN_TASK_CORE_ID TASK_CORE_ID_DEFAULT

// WiFi STA task (init + event task, self-deletes after setup).
// NOTE: esp_wifi_start() triggers full RF PHY calibration on first boot; the
// default 4096-byte stack is enough here because setup is otherwise light, but
// bump this if you ever see a silent stack overflow -> SW_RESET boot loop.
#define WIFI_STA_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define WIFI_STA_TASK_PRIORITY TASK_PRIORITY_DEFAULT
#define WIFI_STA_TASK_CORE_ID TASK_CORE_ID_DEFAULT

#endif // TASK_SETTINGS_H
