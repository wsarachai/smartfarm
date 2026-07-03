#pragma once
// -----------------------------------------------------------------------------
// Access Point configuration for ap-server.
//
// This is a standalone AP ("MJU-SmartFarm-AP-II" @ 192.168.1.1) — a distinct
// network from the ESP-IDF reference in ../esp-idf-iot/web-server (which uses
// "MJU-SmartFarm-AP" @ 192.168.0.1). Clients join THIS AP; sensor-node firmware
// pointed at the reference AP will not reach it.
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
#define AP_MAX_CONNECTIONS 5                      // max simultaneous stations (1..10)

// --- Addressing (comma-separated octets, consumed by IPAddress(...)) ---
// Gateway is intentionally the same as the AP IP.
#define AP_IP      192, 168, 1, 1
#define AP_GATEWAY 192, 168, 1, 1
#define AP_NETMASK 255, 255, 255, 0

// --- DHCP lease pool ---
// Addresses 192.168.1.1 .. .99 are RESERVED for static servers (the AP itself
// sits at .1). The DHCP server leases to clients starting at the host octet
// below; the pool END is computed as (first host + AP_MAX_CONNECTIONS - 1), so
// with 5 max clients the pool is 192.168.1.100 .. .104. The pool always sits on
// the same /24 as AP_IP. (Note: the ESP DHCP server tracks at most ~8 leases.)
#define DHCP_POOL_FIRST_HOST 100  // last octet of the first leasable address

// --- HTTP status page ---
#define HTTP_PORT           80  // browse to http://192.168.1.1/
#define STATUS_REFRESH_SECS 3   // client-side meta-refresh interval
