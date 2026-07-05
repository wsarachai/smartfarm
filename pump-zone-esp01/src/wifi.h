#ifndef WIFI_H
#define WIFI_H
// STA-only WiFi bring-up. ESP8266WiFi handles auto-reconnect internally, so this
// is far simpler than the ESP32 pump-zone's hand-rolled reconnect timer.

#include <stdbool.h>

void wifi_begin(void);        // start joining the AP (non-blocking)
bool wifi_is_connected(void); // true once associated + IP acquired

#endif // WIFI_H
