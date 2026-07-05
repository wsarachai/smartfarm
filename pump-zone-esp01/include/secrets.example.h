// Copy to secrets.h and fill in real values. secrets.h is gitignored.
// This node joins the AP as a STA and gets its IP via DHCP — reserve its MAC in
// ap-server's web UI so it inherits the SAME reserved address the ESP32 pump-zone
// used (it is a drop-in replacement; keep DEVICE_ID = "pump_zone_01").
#ifndef SECRETS_H
#define SECRETS_H
#define WIFI_STA_AP_SSID     "MJU-SmartFarm-AP-II"   // AP to join (from ap-server)
#define WIFI_STA_AP_PASSWORD "password"              // change to your AP password
#define DEVICE_ID            "pump_zone_01"          // reuse the main pump's identity (drop-in)
#define OTA_PASSWORD         "change-me-ota"         // ArduinoOTA upload password
#endif // SECRETS_H
