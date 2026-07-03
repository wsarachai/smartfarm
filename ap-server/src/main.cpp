// -----------------------------------------------------------------------------
// ap-server — ESP-WROOM-32 SoftAP + DHCP + live status page
//
// Re-implements the AP + DHCP behavior of ../esp-idf-iot/web-server on the
// Arduino framework:
//   * Brings up a WPA2 SoftAP (AP-only, no station uplink).
//   * softAPConfig() sets the static AP IP; we then reconfigure the core's DHCP
//     server to lease from 192.168.1.100+, reserving .1-.99 for static servers.
//   * A tiny synchronous WebServer serves one auto-refreshing status page at
//     http://192.168.1.1/ listing every connected station's MAC and leased IP.
//
// All tunables live in include/ap_config.h.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "esp_wifi.h"                 // esp_wifi_ap_get_sta_list()
#include "esp_netif.h"                // esp_netif_get_sta_list(), dhcps options
#include "dhcpserver/dhcpserver.h"    // dhcps_lease_t (uses lwip ip4_addr_t)
#include "lwip/ip4_addr.h"            // IP4_ADDR()

#include "ap_config.h"

// The DHCP pool must fit within the AP's /24, below the broadcast address.
static_assert(DHCP_POOL_FIRST_HOST >= 2 &&
                  DHCP_POOL_FIRST_HOST + AP_MAX_CONNECTIONS - 1 <= 254,
              "DHCP pool (DHCP_POOL_FIRST_HOST .. +AP_MAX_CONNECTIONS-1) "
              "must stay within .2-.254 of the AP subnet");

static const IPAddress kApIp(AP_IP);
static const IPAddress kApGateway(AP_GATEWAY);
static const IPAddress kApNetmask(AP_NETMASK);

// Derived from the AP subnet so the pool always tracks AP_IP's /24.
static const IPAddress kDhcpPoolStart(kApIp[0], kApIp[1], kApIp[2],
                                      DHCP_POOL_FIRST_HOST);
static const IPAddress kDhcpPoolEnd(kApIp[0], kApIp[1], kApIp[2],
                                    DHCP_POOL_FIRST_HOST + AP_MAX_CONNECTIONS - 1);
// Reserved static band is .1 .. (first pool host - 1).
static const IPAddress kReservedEnd(kApIp[0], kApIp[1], kApIp[2],
                                    DHCP_POOL_FIRST_HOST - 1);

static WebServer server(HTTP_PORT);

// Overrides the SoftAP DHCP server's address pool so leases start at
// kDhcpPoolStart instead of the default AP_IP+1. Must run after softAP() (which
// starts the DHCP server); the pool option can only be set while it is stopped.
static bool configureDhcpPool() {
  esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (ap == nullptr) {
    Serial.println("[ap-server] AP netif not found; DHCP pool left at default");
    return false;
  }

  // dhcps_lease_t holds lwip ip4_addr_t; IP4_ADDR sets .addr in network order.
  dhcps_lease_t lease = {};
  lease.enable = true;
  IP4_ADDR(&lease.start_ip, kApIp[0], kApIp[1], kApIp[2], DHCP_POOL_FIRST_HOST);
  IP4_ADDR(&lease.end_ip, kApIp[0], kApIp[1], kApIp[2],
           DHCP_POOL_FIRST_HOST + AP_MAX_CONNECTIONS - 1);

  esp_err_t err = esp_netif_dhcps_stop(ap);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    Serial.printf("[ap-server] dhcps_stop failed: %s\n", esp_err_to_name(err));
  }

  err = esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                               ESP_NETIF_REQUESTED_IP_ADDRESS,
                               &lease, sizeof(lease));
  if (err != ESP_OK) {
    Serial.printf("[ap-server] set DHCP pool failed: %s\n", esp_err_to_name(err));
    esp_netif_dhcps_start(ap);  // restart with whatever pool it had
    return false;
  }

  err = esp_netif_dhcps_start(ap);
  if (err != ESP_OK) {
    Serial.printf("[ap-server] dhcps_start failed: %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

// Formats a 6-byte MAC as AA:BB:CC:DD:EE:FF.
static String macToString(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// Builds the rows of the connected-client table by joining the WiFi driver's
// station list (MACs) against the netif DHCP lease table (assigned IPs).
static String buildClientRows() {
  wifi_sta_list_t wifi_list = {};
  esp_netif_sta_list_t netif_list = {};

  if (esp_wifi_ap_get_sta_list(&wifi_list) != ESP_OK ||
      esp_netif_get_sta_list(&wifi_list, &netif_list) != ESP_OK) {
    return "<tr><td colspan=\"3\">(failed to read station list)</td></tr>";
  }

  if (netif_list.num == 0) {
    return "<tr><td colspan=\"3\">No clients connected yet</td></tr>";
  }

  String rows;
  for (int i = 0; i < netif_list.num; i++) {
    const esp_netif_sta_info_t &sta = netif_list.sta[i];
    IPAddress ip(sta.ip.addr);  // esp_ip4_addr_t is little-endian, IPAddress matches
    rows += "<tr><td>";
    rows += String(i + 1);
    rows += "</td><td>";
    rows += macToString(sta.mac);
    rows += "</td><td>";
    rows += ip.toString();
    rows += "</td></tr>";
  }
  return rows;
}

static void handleStatus() {
  String html;
  html.reserve(2000);
  html += "<!doctype html><html><head><meta charset=\"utf-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<meta http-equiv=\"refresh\" content=\"" + String(STATUS_REFRESH_SECS) + "\">";
  html += "<title>" AP_SSID " — AP Status</title>";
  html += "<style>body{font-family:system-ui,sans-serif;margin:2rem;color:#1a1a1a}"
          "h1{font-size:1.3rem}table{border-collapse:collapse;margin-top:.5rem;width:100%;max-width:520px}"
          "th,td{border:1px solid #ccc;padding:.4rem .6rem;text-align:left;font-size:.9rem}"
          "th{background:#f2f2f2}.k{color:#666}code{background:#f2f2f2;padding:.1rem .3rem;border-radius:3px}</style>";
  html += "</head><body>";
  html += "<h1>" AP_SSID " — Access Point</h1>";

  html += "<p><span class=\"k\">AP IP:</span> <code>" + kApIp.toString() + "</code>";
  html += " &nbsp; <span class=\"k\">Channel:</span> " + String(AP_CHANNEL);
  html += " &nbsp; <span class=\"k\">Clients:</span> " + String(WiFi.softAPgetStationNum());
  html += " / " + String(AP_MAX_CONNECTIONS) + "</p>";

  html += "<p><span class=\"k\">Reserved (static):</span> <code>" + kApIp.toString() +
          "&ndash;" + kReservedEnd.toString() + "</code>";
  html += " &nbsp; <span class=\"k\">DHCP pool:</span> <code>" + kDhcpPoolStart.toString() +
          "&ndash;" + kDhcpPoolEnd.toString() + "</code></p>";

  html += "<table><thead><tr><th>#</th><th>MAC</th><th>Leased IP</th></tr></thead><tbody>";
  html += buildClientRows();
  html += "</tbody></table>";

  html += "<p class=\"k\">Auto-refreshing every " + String(STATUS_REFRESH_SECS) + "s.</p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[ap-server] starting SoftAP...");

  WiFi.mode(WIFI_AP);

  // Set the static AP IP BEFORE softAP(). The DHCP pool is then overridden
  // below (configureDhcpPool) to start at kDhcpPoolStart, not the default .2.
  if (!WiFi.softAPConfig(kApIp, kApGateway, kApNetmask)) {
    Serial.println("[ap-server] softAPConfig() FAILED");
  }

  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL,
                        AP_SSID_HIDDEN, AP_MAX_CONNECTIONS);
  if (!ok) {
    Serial.println("[ap-server] softAP() FAILED — halting");
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("[ap-server] AP up: SSID=\"%s\"  IP=%s  channel=%d  max=%d\n",
                AP_SSID, WiFi.softAPIP().toString().c_str(),
                AP_CHANNEL, AP_MAX_CONNECTIONS);

  if (configureDhcpPool()) {
    Serial.printf("[ap-server] Reserved (static): %s-%s   DHCP pool: %s-%s\n",
                  kApIp.toString().c_str(), kReservedEnd.toString().c_str(),
                  kDhcpPoolStart.toString().c_str(), kDhcpPoolEnd.toString().c_str());
  }

  server.on("/", handleStatus);
  server.onNotFound(handleStatus);  // any path shows the status page
  server.begin();
  Serial.printf("[ap-server] HTTP status page at http://%s/\n",
                WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
}
