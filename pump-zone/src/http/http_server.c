#include "http_server.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "pump_config.h" // PUMP_HTTP_PORT
#include "relay.h"
#include "secrets.h"
#include "wifi_sta.h"

static const char TAG[] = "http_server";
static httpd_handle_t s_server = NULL;

// GET /favicon.ico -> silence browser auto-fetches with no-content response.
static esp_err_t http_server_favicon_get_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "GET /favicon.ico requested");
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
  return httpd_resp_send(req, NULL, 0);
}

// GET / -> minimal device health/status payload for quick diagnostics.
static esp_err_t http_server_default_get_handler(httpd_req_t *req)
{
  bool wifi_connected = wifi_sta_is_connected();
  bool server_running = http_server_is_running();

  char ip[16] = "unavailable";
  esp_err_t ip_err = wifi_sta_get_ip_str(ip, sizeof(ip));
  if (ip_err != ESP_OK)
  {
    ESP_LOGW(TAG, "GET / health requested, IP not available yet: %s", esp_err_to_name(ip_err));
  }

  const char *relay_state = relay_get_state() ? "ON" : "OFF";
  const char *health_status = (wifi_connected && server_running) ? "ok" : "degraded";
  unsigned long long uptime_ms = (unsigned long long)(esp_timer_get_time() / 1000ULL);

  char json_response[256];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"device_id\":\"%s\",\"health\":{\"status\":\"%s\",\"wifi_connected\":%s,\"http_server\":%s,\"relay\":\"%s\",\"ip\":\"%s\",\"uptime_ms\":%llu}}",
      DEVICE_ID,
      health_status,
      wifi_connected ? "true" : "false",
      server_running ? "true" : "false",
      relay_state,
      ip,
      uptime_ms);

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    ESP_LOGE(TAG, "Failed to create health JSON response");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG,
           "GET / health -> status=%s wifi=%s relay=%s ip=%s uptime_ms=%llu",
           health_status,
           wifi_connected ? "true" : "false",
           relay_state,
           ip,
           uptime_ms);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_register_default_handlers(httpd_handle_t server)
{
  httpd_uri_t favicon_get = {
      .uri = "/favicon.ico",
      .method = HTTP_GET,
      .handler = http_server_favicon_get_handler,
      .user_ctx = NULL,
  };

  esp_err_t err = httpd_register_uri_handler(server, &favicon_get);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t root_get = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = http_server_default_get_handler,
      .user_ctx = NULL,
  };

  return httpd_register_uri_handler(server, &root_get);
}

esp_err_t http_server_start(void)
{
  if (s_server != NULL)
  {
    ESP_LOGI(TAG, "HTTP server already running");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = PUMP_HTTP_PORT;
  config.max_uri_handlers = 8;
  config.max_open_sockets = 7;
  config.backlog_conn = 8;
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 3;
  config.send_wait_timeout = 8;
  config.keep_alive_enable = false;

  ESP_LOGI(TAG,
           "Starting HTTP server on port %d (max_sockets=%u backlog=%u lru_purge=%s)",
           config.server_port,
           (unsigned int)config.max_open_sockets,
           (unsigned int)config.backlog_conn,
           config.lru_purge_enable ? "true" : "false");

  esp_err_t err = httpd_start(&s_server, &config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
    return err;
  }

  err = http_server_register_default_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register default handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  err = http_server_register_relay_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register relay handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}

bool http_server_is_running(void)
{
  return (s_server != NULL);
}
