// Copy to secrets.h and fill in real values. secrets.h is gitignored.
// The pump node joins the AP as a STA and gets its IP via DHCP — reserve its MAC
// in ap-server's web UI for a stable .2–.99 (server group) address.
#ifndef SECRETS_H
#define SECRETS_H
#define WIFI_STA_AP_SSID     "MJU-SmartFarm-AP-II"   // AP to join (from ap-server)
#define WIFI_STA_AP_PASSWORD "password"              // change to your AP password
#define DEVICE_ID            "pump_zone_01"          // this node's identity
#endif // SECRETS_H
