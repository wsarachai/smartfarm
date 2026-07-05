#include "wifi.h"

#include <ESP8266WiFi.h>

#include "pump_config.h"  // USE_STATIC_IP, STATIC_IP_ADDR, ...
#include "secrets.h"      // WIFI_STA_AP_SSID, WIFI_STA_AP_PASSWORD, DEVICE_ID

void wifi_begin(void) {
  WiFi.persistent(false);        // don't wear flash rewriting credentials each boot
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);   // driver reconnects automatically on drop
  WiFi.hostname(DEVICE_ID);      // shows up in the AP/DHCP client list

#if USE_STATIC_IP
  // Must be set BEFORE begin(). Claims the fixed address instead of DHCP.
  IPAddress ip(STATIC_IP_ADDR);
  IPAddress gw(STATIC_GATEWAY);
  IPAddress sn(STATIC_SUBNET);
  IPAddress dns(STATIC_DNS);
  if (!WiFi.config(ip, gw, sn, dns)) {
    Serial.println("[wifi] static IP config FAILED — falling back to DHCP");
  } else {
    Serial.printf("[wifi] static IP %s\n", ip.toString().c_str());
  }
#endif

  WiFi.begin(WIFI_STA_AP_SSID, WIFI_STA_AP_PASSWORD);
  Serial.printf("[wifi] joining \"%s\" as \"%s\"...\n", WIFI_STA_AP_SSID, DEVICE_ID);
}

bool wifi_is_connected(void) {
  return WiFi.status() == WL_CONNECTED;
}
