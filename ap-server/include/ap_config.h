#pragma once
// -----------------------------------------------------------------------------
// Access Point configuration for ap-server.
//
// This is a standalone AP ("MJU-SmartFarm-AP-II" @ 192.168.0.1). Clients join
// THIS AP; sensor-node firmware pointed at the reference AP will not reach it.
//
// None of this is truly secret: a WPA2 pre-shared key is shared with every
// client that joins, so it lives in a committed header (no secrets.h needed).
// Change AP_PASSWORD to something stronger before any real deployment.
// -----------------------------------------------------------------------------

// --- Identity / security ---
#define AP_SSID            "MJU-SmartFarm-AP-II"  // AP name (broadcast SSID)
#define AP_PASSWORD        "password"             // WPA2-PSK, min 8 chars (empty => open)
#define AP_CHANNEL         1                      // 1..13
#define AP_SSID_HIDDEN     0                      // 0 = broadcast SSID, 1 = hidden
#define AP_MAX_CONNECTIONS 10                     // max simultaneous stations (ESP32 hw max is 10)

// --- Addressing (comma-separated octets, consumed by IPAddress(...)) ---
// Gateway is intentionally the same as the AP IP.
#define AP_IP      192, 168, 0, 1
#define AP_GATEWAY 192, 168, 0, 1
#define AP_NETMASK 255, 255, 255, 0

// --- Addressing scheme (served by our custom DHCP server, not the built-in) ---
//   .1                         : the AP itself
//   .2 .. (POOL_FIRST_HOST-1)  : RESERVED band — MAC->IP reservations (the
//                                "server group"), assigned via the web UI
//   POOL_FIRST_HOST .. +max-1  : DYNAMIC pool for unreserved clients
// With POOL_FIRST_HOST=100 and 10 max clients: reserved .2-.99, dynamic .100-.109.
#define DHCP_POOL_FIRST_HOST 100    // last octet of the first DYNAMIC address
#define DHCP_LEASE_SECS      7200   // lease time handed to clients (2 hours)

// --- MAC->IP reservations (persisted in NVS) ---
#define MAX_RESERVATIONS        32  // capacity of the reservation table
#define RESERVATION_LABEL_MAXLEN 24 // friendly-name length (excl. NUL)

// --- HTTP status page ---
#define HTTP_PORT           80  // browse to http://192.168.0.1/
#define STATUS_REFRESH_SECS 5   // client-side meta-refresh interval
