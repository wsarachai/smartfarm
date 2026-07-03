#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Events posted from the WiFi STA layer (wifi_sta.c) to the main task.
// Kept identical to sensor-zone so wifi_sta.c is reused verbatim.
typedef enum
{
  APP_MSG_WIFI_CONNECTED_GOT_IP = 0,
  APP_MSG_WIFI_DISCONNECTED,
} app_event_id_t;

typedef struct
{
  app_event_id_t event_id;
} app_event_t;

// Queue handle used to post events to the main task.
extern QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t event_id);

#endif // MAIN_H
