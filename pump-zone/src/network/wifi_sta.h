#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>

// Target AP credentials live in secrets.h (gitignored). Copy
// include/secrets.example.h -> include/secrets.h and fill in your values.
// Provides: WIFI_STA_AP_SSID, WIFI_STA_AP_PASSWORD, DEVICE_ID.
#include "secrets.h"

/**
 * @brief Starts WiFi in STA-only mode and connects to WIFI_STA_AP_SSID.
 *        Spawns an internal FreeRTOS task that initialises the network stack,
 *        connects to the AP, and self-deletes after configuration.
 *        Connection events are forwarded to the main-task queue via
 *        app_send_message().
 */
void wifi_sta_start(void);

/**
 * @brief Returns true when the STA interface has a valid IP address.
 */
bool wifi_sta_is_connected(void);

/**
 * @brief Forces a WiFi disconnect + reconnect cycle.
 */
void wifi_sta_force_reconnect(void);

#endif // WIFI_STA_H
