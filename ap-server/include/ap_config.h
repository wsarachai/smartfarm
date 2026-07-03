#pragma once
// -----------------------------------------------------------------------------
// Access Point configuration for ap-server.
//
// These values MIRROR ../esp-idf-iot/web-server/main/network/wifi_app.h so that
// existing sensor-node firmware expecting "MJU-SmartFarm-AP" at 192.168.0.1 can
// associate unchanged.
//
// None of this is truly secret: a WPA2 pre-shared key is shared with every
// client that joins, so it lives in a committed header (no secrets.h needed).
// Change AP_PASSWORD to something stronger before any real deployment.
// -----------------------------------------------------------------------------

// --- Identity / security ---
#define AP_SSID            "MJU-SmartFarm-AP"  // AP name (broadcast SSID)
#define AP_PASSWORD        "password"          // WPA2-PSK, min 8 chars (empty => open)
#define AP_CHANNEL         1                   // 1..13
#define AP_SSID_HIDDEN     0                   // 0 = broadcast SSID, 1 = hidden
#define AP_MAX_CONNECTIONS 5                   // max simultaneous stations (1..10)

// --- Addressing (comma-separated octets, consumed by IPAddress(...)) ---
// Gateway is intentionally the same as the AP IP; the DHCP lease pool is
// derived automatically starting at AP_IP + 1 (192.168.0.2 onward).
#define AP_IP      192, 168, 0, 1
#define AP_GATEWAY 192, 168, 0, 1
#define AP_NETMASK 255, 255, 255, 0

// --- HTTP status page ---
#define HTTP_PORT           80  // browse to http://192.168.0.1/
#define STATUS_REFRESH_SECS 3   // client-side meta-refresh interval
