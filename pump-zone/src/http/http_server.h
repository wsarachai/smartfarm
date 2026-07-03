#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t http_server_start(void);
bool http_server_is_running(void);

esp_err_t http_server_register_relay_handlers(httpd_handle_t server);

#endif // HTTP_SERVER_H
