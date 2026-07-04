#include "http_server.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "relay.h"

static const char TAG[] = "http_server_relay";

// Max accepted request body. The only expected payload is {"state":"on"|"off"}.
#define RELAY_BODY_MAX_LEN 128

// Sends {"relay_status":"ON|OFF"} reflecting the current relay state.
static esp_err_t send_relay_status(httpd_req_t *req)
{
  const bool relay_on = relay_get_state();
  char json_response[40];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"relay_status\":\"%s\"}",
      relay_on ? "ON" : "OFF");

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    ESP_LOGE(TAG, "Failed to create relay status JSON response");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Responding to %s %s with relay_status=%s",
           req->method == HTTP_GET ? "GET" : (req->method == HTTP_POST ? "POST" : "HTTP"),
           req->uri,
           relay_on ? "ON" : "OFF");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

// GET /api/v1/relay -> current pump state.
static esp_err_t http_server_relay_get_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "GET %s requested", req->uri);
  return send_relay_status(req);
}

// POST /api/v1/relay  body: {"state":"on"|"off"} -> switches the pump.
static esp_err_t http_server_relay_post_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "POST %s requested (content_len=%d)", req->uri, req->content_len);

  if (req->content_len <= 0 || req->content_len >= RELAY_BODY_MAX_LEN)
  {
    ESP_LOGE(TAG, "Invalid body length: %d", req->content_len);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body must be a small JSON object");
    return ESP_FAIL;
  }

  char body[RELAY_BODY_MAX_LEN] = {0};
  int received = httpd_req_recv(req, body, req->content_len);
  if (received <= 0)
  {
    if (received == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timed out");
    }
    else
    {
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
    }
    return ESP_FAIL;
  }
  body[received] = '\0';
  ESP_LOGI(TAG, "POST body: %s", body);

  cJSON *root = cJSON_Parse(body);
  if (root == NULL)
  {
    ESP_LOGE(TAG, "Malformed JSON body");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed JSON");
    return ESP_FAIL;
  }

  const cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
  if (!cJSON_IsString(state) || state->valuestring == NULL)
  {
    ESP_LOGE(TAG, "Missing or non-string \"state\" field");
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected {\"state\":\"on\"|\"off\"}");
    return ESP_FAIL;
  }

  bool enable;
  if (strcmp(state->valuestring, "on") == 0)
  {
    enable = true;
  }
  else if (strcmp(state->valuestring, "off") == 0)
  {
    enable = false;
  }
  else
  {
    ESP_LOGE(TAG, "Unknown state value: %s", state->valuestring);
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "state must be \"on\" or \"off\"");
    return ESP_FAIL;
  }

  cJSON_Delete(root);

  ESP_LOGI(TAG, "Applying relay state: %s", enable ? "ON" : "OFF");
  esp_err_t err = relay_set(enable);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to set relay: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to control relay");
    return ESP_FAIL;
  }

  return send_relay_status(req);
}

esp_err_t http_server_register_relay_handlers(httpd_handle_t server)
{
  httpd_uri_t relay_get = {
      .uri = "/api/v1/relay",
      .method = HTTP_GET,
      .handler = http_server_relay_get_handler,
      .user_ctx = NULL,
  };
  esp_err_t err = httpd_register_uri_handler(server, &relay_get);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t relay_post = {
      .uri = "/api/v1/relay",
      .method = HTTP_POST,
      .handler = http_server_relay_post_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &relay_post);
}
