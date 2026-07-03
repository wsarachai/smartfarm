#include "wifi_sta.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

#include "main.h"
#include "task_settings.h"

static const char *TAG = "wifi_sta";

static EventGroupHandle_t s_event_group;
static volatile bool s_connected = false;
static esp_timer_handle_t s_reconnect_timer = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_RECONNECT_DELAY_US (1000 * 1000) // 1 second

static const char *wifi_disc_reason_to_str(uint8_t reason)
{
  switch (reason)
  {
  case WIFI_REASON_AUTH_EXPIRE:
    return "AUTH_EXPIRE";
  case WIFI_REASON_AUTH_LEAVE:
    return "AUTH_LEAVE";
  case WIFI_REASON_ASSOC_EXPIRE:
    return "ASSOC_EXPIRE";
  case WIFI_REASON_ASSOC_TOOMANY:
    return "ASSOC_TOOMANY";
  case WIFI_REASON_NOT_AUTHED:
    return "NOT_AUTHED";
  case WIFI_REASON_NOT_ASSOCED:
    return "NOT_ASSOCED";
  case WIFI_REASON_ASSOC_LEAVE:
    return "ASSOC_LEAVE";
  case WIFI_REASON_ASSOC_NOT_AUTHED:
    return "ASSOC_NOT_AUTHED";
  case WIFI_REASON_DISASSOC_PWRCAP_BAD:
    return "DISASSOC_PWRCAP_BAD";
  case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
    return "DISASSOC_SUPCHAN_BAD";
  case WIFI_REASON_IE_INVALID:
    return "IE_INVALID";
  case WIFI_REASON_MIC_FAILURE:
    return "MIC_FAILURE";
  case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    return "4WAY_HANDSHAKE_TIMEOUT";
  case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    return "GROUP_KEY_UPDATE_TIMEOUT";
  case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    return "IE_IN_4WAY_DIFFERS";
  case WIFI_REASON_GROUP_CIPHER_INVALID:
    return "GROUP_CIPHER_INVALID";
  case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
    return "PAIRWISE_CIPHER_INVALID";
  case WIFI_REASON_AKMP_INVALID:
    return "AKMP_INVALID";
  case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
    return "UNSUPP_RSN_IE_VERSION";
  case WIFI_REASON_INVALID_RSN_IE_CAP:
    return "INVALID_RSN_IE_CAP";
  case WIFI_REASON_802_1X_AUTH_FAILED:
    return "802_1X_AUTH_FAILED";
  case WIFI_REASON_CIPHER_SUITE_REJECTED:
    return "CIPHER_SUITE_REJECTED";
  case WIFI_REASON_BEACON_TIMEOUT:
    return "BEACON_TIMEOUT";
  case WIFI_REASON_NO_AP_FOUND:
    return "NO_AP_FOUND";
  case WIFI_REASON_AUTH_FAIL:
    return "AUTH_FAIL";
  case WIFI_REASON_ASSOC_FAIL:
    return "ASSOC_FAIL";
  case WIFI_REASON_HANDSHAKE_TIMEOUT:
    return "HANDSHAKE_TIMEOUT";
  case WIFI_REASON_CONNECTION_FAIL:
    return "CONNECTION_FAIL";
  case WIFI_REASON_AP_TSF_RESET:
    return "AP_TSF_RESET";
  case WIFI_REASON_ROAMING:
    return "ROAMING";
  default:
    return "UNKNOWN";
  }
}

static void reconnect_timer_cb(void *arg)
{
  ESP_LOGI(TAG, "Reconnecting to \"%s\"...", WIFI_STA_AP_SSID);
  esp_wifi_connect();
}

bool wifi_sta_is_connected(void)
{
  return s_connected;
}

// ---------------------------------------------------------------------------
// Event handler (runs in the WiFi/IP event loop task)
// ---------------------------------------------------------------------------

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
  if (base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "STA started, connecting to \"%s\"...", WIFI_STA_AP_SSID);
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "STA connected to AP");
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
    {
      wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
      s_connected = false;
      app_send_message(APP_MSG_WIFI_DISCONNECTED);
      ESP_LOGW(TAG,
               "STA disconnected (reason=%u:%s) — retrying in 1s...",
               (unsigned)disc->reason,
               wifi_disc_reason_to_str((uint8_t)disc->reason));

      if (disc->reason == WIFI_REASON_AUTH_FAIL ||
          disc->reason == WIFI_REASON_AUTH_EXPIRE ||
          disc->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
          disc->reason == WIFI_REASON_HANDSHAKE_TIMEOUT)
      {
        ESP_LOGW(TAG,
                 "Likely credential/security mismatch. Check SSID/password and AP auth mode.");
      }
      // Stop any pending timer before (re-)scheduling to avoid double-fire.
      esp_timer_stop(s_reconnect_timer);
      esp_timer_start_once(s_reconnect_timer, WIFI_RECONNECT_DELAY_US);
      break;
    }

    default:
      break;
    }
  }
  else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
    s_connected = true;
    xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    app_send_message(APP_MSG_WIFI_CONNECTED_GOT_IP);
  }
}

// ---------------------------------------------------------------------------
// Init task — self-deletes after WiFi driver setup
// ---------------------------------------------------------------------------

static void wifi_sta_task(void *pvParameters)
{
  s_event_group = xEventGroupCreate();

  // 1. Init NVS (required by the WiFi driver)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGW(TAG, "Erasing NVS flash");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 2. Initialise TCP/IP stack and default event loop
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  // 3. Initialise the WiFi driver
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // 4. Create the reconnect timer (one-shot, restarted on each disconnect).
  const esp_timer_create_args_t timer_args = {
      .callback = reconnect_timer_cb,
      .name = "wifi_reconnect",
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));

  // 5. Register event handlers
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

  // 6. Configure STA with the target AP credentials
  wifi_config_t sta_cfg = {
      .sta = {
          .ssid = WIFI_STA_AP_SSID,
          .password = WIFI_STA_AP_PASSWORD,
          // Accept WPA and above to tolerate APs configured as mixed WPA/WPA2.
          .threshold.authmode = WIFI_AUTH_WPA_PSK,
          .scan_method = WIFI_ALL_CHANNEL_SCAN,
          .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
          .pmf_cfg = {
              // Some legacy APs mis-handle PMF capability advertisements.
              .capable = false,
              .required = false,
          },
      },
  };

  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Wait until connected (or forever — reconnect is automatic)
  xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT,
                      pdFALSE, pdFALSE, portMAX_DELAY);

  ESP_LOGI(TAG, "Initial connection to \"%s\" established", WIFI_STA_AP_SSID);
  vTaskDelete(NULL);
}

void wifi_sta_force_reconnect(void)
{
  if (!s_connected)
  {
    return; // already cycling, reconnect timer will handle it
  }
  ESP_LOGW(TAG, "Server unreachable — forcing WiFi reconnect");
  s_connected = false;
  // Disconnect fires WIFI_EVENT_STA_DISCONNECTED -> reconnect timer -> esp_wifi_connect()
  esp_wifi_disconnect();
}

void wifi_sta_start(void)
{
  xTaskCreatePinnedToCore(
      wifi_sta_task,
      "wifi_sta_task",
      WIFI_STA_TASK_STACK_SIZE,
      NULL,
      WIFI_STA_TASK_PRIORITY,
      NULL,
      WIFI_STA_TASK_CORE_ID);
}
