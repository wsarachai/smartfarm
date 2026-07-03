// -----------------------------------------------------------------------------
// ap-server — ESP-WROOM-32 SoftAP + DHCP + live status page
//
// Re-implements the AP + DHCP behavior of ../esp-idf-iot/web-server on the
// Arduino framework:
//   * Brings up a WPA2 SoftAP (AP-only, no station uplink).
//   * softAPConfig() sets the static AP IP; the Arduino-ESP32 core auto-starts
//     a DHCP server on the AP interface, handing out 192.168.1.2+ to clients.
//   * A tiny synchronous WebServer serves one auto-refreshing status page at
//     http://192.168.1.1/ listing every connected station's MAC and leased IP.
//
// All tunables live in include/ap_config.h.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "esp_wifi.h"    // esp_wifi_ap_get_sta_list()
#include "esp_netif.h"   // esp_netif_get_sta_list()

#include "ap_config.h"

static const IPAddress kApIp(AP_IP);
static const IPAddress kApGateway(AP_GATEWAY);
static const IPAddress kApNetmask(AP_NETMASK);

static WebServer server(HTTP_PORT);

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
  html.reserve(1600);
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

  // Set the static AP IP BEFORE softAP() so the core's DHCP server derives its
  // lease pool from this gateway (clients get 192.168.1.2+).
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

  server.on("/", handleStatus);
  server.onNotFound(handleStatus);  // any path shows the status page
  server.begin();
  Serial.printf("[ap-server] HTTP status page at http://%s/\n",
                WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
}
