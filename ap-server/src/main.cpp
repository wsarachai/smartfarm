// -----------------------------------------------------------------------------
// ap-server — ESP-WROOM-32 SoftAP + custom DHCP (with MAC reservations) + web UI
//
//   * Brings up a WPA2 SoftAP (AP-only, no station uplink).
//   * Stops the built-in DHCP server and runs our own (dhcp_server.*), which
//     honours MAC->IP reservations in the .2-.99 band and hands dynamic leases
//     from .100+ to everyone else.
//   * Serves a web UI: a live dashboard (connected clients + reservations) and
//     an /edit form to add/update reservations, persisted in NVS.
//
// All tunables live in include/ap_config.h.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "esp_wifi.h"     // esp_wifi_ap_get_sta_list()
#include "esp_netif.h"    // esp_netif_dhcps_stop()

#include "ap_config.h"
#include "dhcp_server.h"
#include "reservations.h"

// Dynamic pool bounds (host octets) and the reserved band just below it.
static const uint8_t kPoolFirst = DHCP_POOL_FIRST_HOST;
static const uint8_t kPoolLast = DHCP_POOL_FIRST_HOST + AP_MAX_CONNECTIONS - 1;

// The pool must fit within the AP's /24, below the broadcast address.
static_assert(kPoolFirst >= 3 && kPoolLast <= 254,
              "Dynamic DHCP pool must stay within .3-.254 (leave .2+ reserved)");

static const IPAddress kApIp(AP_IP);
static const IPAddress kApGateway(AP_GATEWAY);
static const IPAddress kApNetmask(AP_NETMASK);

static const IPAddress kReservedLow(kApIp[0], kApIp[1], kApIp[2], 2);
static const IPAddress kReservedHigh(kApIp[0], kApIp[1], kApIp[2], kPoolFirst - 1);
static const IPAddress kDynLow(kApIp[0], kApIp[1], kApIp[2], kPoolFirst);
static const IPAddress kDynHigh(kApIp[0], kApIp[1], kApIp[2], kPoolLast);

static WebServer server(HTTP_PORT);

// --- HTML helpers -----------------------------------------------------------

static String esc(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      case '\'': o += "&#39;"; break;
      default: o += c;
    }
  }
  return o;
}

static const char *kStyle =
    "<style>body{font-family:system-ui,sans-serif;margin:1.5rem;color:#1a1a1a}"
    "h1{font-size:1.3rem}h2{font-size:1.05rem;margin-top:1.5rem}"
    "table{border-collapse:collapse;margin-top:.4rem;width:100%;max-width:640px}"
    "th,td{border:1px solid #ccc;padding:.35rem .55rem;text-align:left;font-size:.9rem}"
    "th{background:#f2f2f2}.k{color:#666}code{background:#f2f2f2;padding:.1rem .3rem;border-radius:3px}"
    "a.btn,button{font-size:.85rem;padding:.2rem .5rem;border:1px solid #888;border-radius:4px;"
    "background:#fafafa;cursor:pointer;text-decoration:none;color:#1a1a1a}"
    "form.inline{display:inline;margin:0}label{display:block;margin:.6rem 0 .15rem}"
    "input{font-size:.95rem;padding:.3rem;width:16rem;max-width:100%}"
    ".err{background:#fde8e8;border:1px solid #f5a5a5;padding:.5rem .7rem;border-radius:4px;max-width:640px}"
    ".ok{color:#2a7}</style>";

// --- Dashboard --------------------------------------------------------------

static String connectedRows() {
  wifi_sta_list_t stas = {};
  if (esp_wifi_ap_get_sta_list(&stas) != ESP_OK || stas.num == 0) {
    return "<tr><td colspan=\"5\">No clients connected</td></tr>";
  }

  String rows;
  for (int i = 0; i < stas.num; i++) {
    const uint8_t *mac = stas.sta[i].mac;
    String macStr = macToStr(mac);

    IPAddress ip;
    String ipStr = dhcp::ipForMac(mac, ip) ? ip.toString() : "(pending)";

    const Reservation *r = reservations::findByMac(mac);
    String name;
    String kind;
    String action;
    if (r != nullptr) {
      name = r->label[0] ? esc(String(r->label)) : String("<span class=\"k\">(reserved)</span>");
      kind = "<span class=\"ok\">reserved</span>";
      action = "<a class=\"btn\" href=\"/edit?mac=" + macStr + "\">Edit</a>";
    } else {
      String host = dhcp::hostnameForMac(mac);
      name = host.length() ? esc(host) : String("<span class=\"k\">-</span>");
      kind = "dynamic";
      action = "<a class=\"btn\" href=\"/edit?mac=" + macStr + "\">Reserve</a>";
    }

    rows += "<tr><td>" + String(i + 1) + "</td><td>" + name + "</td><td>" + kind +
            "</td><td><code>" + ipStr + "</code> " + macStr + "</td><td>" + action +
            "</td></tr>";
  }
  return rows;
}

static String reservationRows() {
  int n = reservations::count();
  if (n == 0) {
    return "<tr><td colspan=\"4\">No reservations yet</td></tr>";
  }
  const Reservation *list = reservations::all();
  String rows;
  for (int i = 0; i < n; i++) {
    String macStr = macToStr(list[i].mac);
    IPAddress ip(kApIp[0], kApIp[1], kApIp[2], list[i].octet);
    String label = list[i].label[0] ? esc(String(list[i].label))
                                     : String("<span class=\"k\">-</span>");
    rows += "<tr><td>" + label + "</td><td>" + macStr + "</td><td><code>" +
            ip.toString() + "</code></td><td>";
    rows += "<a class=\"btn\" href=\"/edit?mac=" + macStr + "\">Edit</a> ";
    rows += "<form class=\"inline\" method=\"POST\" action=\"/api/reservations/delete\" "
            "onsubmit=\"return confirm('Delete this reservation?')\">"
            "<input type=\"hidden\" name=\"mac\" value=\"" + macStr + "\">"
            "<button>Delete</button></form>";
    rows += "</td></tr>";
  }
  return rows;
}

static void handleDashboard() {
  String html;
  html.reserve(4096);
  html += "<!doctype html><html><head><meta charset=\"utf-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<meta http-equiv=\"refresh\" content=\"" + String(STATUS_REFRESH_SECS) + "\">";
  html += "<title>" AP_SSID " — AP</title>";
  html += kStyle;
  html += "</head><body>";
  html += "<h1>" AP_SSID " — Access Point</h1>";

  html += "<p><span class=\"k\">AP IP:</span> <code>" + kApIp.toString() + "</code>";
  html += " &nbsp; <span class=\"k\">Channel:</span> " + String(AP_CHANNEL);
  html += " &nbsp; <span class=\"k\">Clients:</span> " + String(WiFi.softAPgetStationNum()) +
          " / " + String(AP_MAX_CONNECTIONS) + "</p>";
  html += "<p><span class=\"k\">Reserved (static):</span> <code>" + kReservedLow.toString() +
          "&ndash;" + kReservedHigh.toString() + "</code>";
  html += " &nbsp; <span class=\"k\">DHCP pool:</span> <code>" + kDynLow.toString() +
          "&ndash;" + kDynHigh.toString() + "</code></p>";

  html += "<h2>Connected clients</h2>";
  html += "<table><thead><tr><th>#</th><th>Name</th><th>Type</th>"
          "<th>IP / MAC</th><th></th></tr></thead><tbody>";
  html += connectedRows();
  html += "</tbody></table>";

  html += "<h2>Reservations (" + String(reservations::count()) + "/" +
          String(MAX_RESERVATIONS) + ") &nbsp; <a class=\"btn\" href=\"/edit\">+ Add</a></h2>";
  html += "<table><thead><tr><th>Label</th><th>MAC</th><th>IP</th><th></th></tr></thead><tbody>";
  html += reservationRows();
  html += "</tbody></table>";

  html += "<p class=\"k\">Dashboard auto-refreshes every " + String(STATUS_REFRESH_SECS) +
          "s. Reservation changes apply on the device's next reconnect/renewal.</p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// --- Edit form (add / update reservation) -----------------------------------

static String defaultIpString() {
  uint8_t o = reservations::nextFreeOctet();
  if (o == 0) {
    o = 2;  // band full; show the low end so the field isn't empty
  }
  return IPAddress(kApIp[0], kApIp[1], kApIp[2], o).toString();
}

static void renderEditPage(const String &errMsg, const String &macVal,
                           const String &ipVal, const String &labelVal) {
  String html;
  html.reserve(2048);
  html += "<!doctype html><html><head><meta charset=\"utf-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<title>Reservation — " AP_SSID "</title>";
  html += kStyle;
  html += "</head><body>";
  html += "<h1>Add / update reservation</h1>";
  if (errMsg.length()) {
    html += "<p class=\"err\">" + esc(errMsg) + "</p>";
  }
  html += "<form method=\"POST\" action=\"/api/reservations\">";
  html += "<label>Label (optional)</label>";
  html += "<input name=\"label\" maxlength=\"" + String(RESERVATION_LABEL_MAXLEN) +
          "\" value=\"" + esc(labelVal) + "\" placeholder=\"e.g. Water Pump\">";
  html += "<label>MAC address</label>";
  html += "<input name=\"mac\" required value=\"" + esc(macVal) +
          "\" placeholder=\"AA:BB:CC:DD:EE:FF\">";
  html += "<label>Assigned IP (must be " + kReservedLow.toString() + "&ndash;" +
          kReservedHigh.toString() + ")</label>";
  html += "<input name=\"ip\" required value=\"" + esc(ipVal) + "\">";
  html += "<p><button type=\"submit\">Save</button> &nbsp; "
          "<a class=\"btn\" href=\"/\">Cancel</a></p>";
  html += "</form></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

static void handleEditForm() {
  String macVal = server.hasArg("mac") ? server.arg("mac") : "";
  String ipVal;
  String labelVal;

  uint8_t mac[6];
  if (macVal.length() && parseMac(macVal.c_str(), mac)) {
    const Reservation *r = reservations::findByMac(mac);
    if (r != nullptr) {
      ipVal = IPAddress(kApIp[0], kApIp[1], kApIp[2], r->octet).toString();
      labelVal = String(r->label);
    }
  }
  if (ipVal.length() == 0) {
    ipVal = defaultIpString();
  }
  renderEditPage("", macVal, ipVal, labelVal);
}

static void handlePostReservation() {
  String macVal = server.arg("mac");
  String ipVal = server.arg("ip");
  String labelVal = server.arg("label");

  uint8_t mac[6];
  if (!parseMac(macVal.c_str(), mac)) {
    renderEditPage("Invalid MAC address.", macVal, ipVal, labelVal);
    return;
  }

  IPAddress ip;
  if (!ip.fromString(ipVal) || ip[0] != kApIp[0] || ip[1] != kApIp[1] ||
      ip[2] != kApIp[2]) {
    renderEditPage("IP must be on the " + kApIp.toString() + "/24 network.", macVal,
                   ipVal, labelVal);
    return;
  }

  ResvResult r = reservations::upsert(mac, ip[3], labelVal.c_str());
  if (r != RESV_OK) {
    renderEditPage(reservations::resultMessage(r), macVal, ipVal, labelVal);
    return;
  }

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handlePostDelete() {
  uint8_t mac[6];
  if (parseMac(server.arg("mac").c_str(), mac)) {
    reservations::removeByMac(mac);
  }
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// --- Setup / loop -----------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[ap-server] starting SoftAP...");

  reservations::begin();
  Serial.printf("[ap-server] loaded %d reservation(s) from NVS\n",
                reservations::count());

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAPConfig(kApIp, kApGateway, kApNetmask)) {
    Serial.println("[ap-server] softAPConfig() FAILED");
  }
  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_SSID_HIDDEN,
                        AP_MAX_CONNECTIONS);
  if (!ok) {
    Serial.println("[ap-server] softAP() FAILED — halting");
    while (true) {
      delay(1000);
    }
  }

  // Stop the built-in DHCP server; our custom server takes over port 67.
  esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (ap != nullptr) {
    esp_err_t st = esp_netif_dhcps_stop(ap);
    Serial.printf("[ap-server] built-in DHCP stop: %s\n", esp_err_to_name(st));
  } else {
    Serial.println("[ap-server] WARNING: AP netif not found; built-in DHCP may clash");
  }

  dhcp::begin(kApIp, kApNetmask, kPoolFirst, kPoolLast, DHCP_LEASE_SECS);

  Serial.printf("[ap-server] AP up: SSID=\"%s\"  IP=%s  channel=%d  max=%d\n",
                AP_SSID, WiFi.softAPIP().toString().c_str(), AP_CHANNEL,
                AP_MAX_CONNECTIONS);
  Serial.printf("[ap-server] Reserved (static): %s-%s   DHCP pool: %s-%s\n",
                kReservedLow.toString().c_str(), kReservedHigh.toString().c_str(),
                kDynLow.toString().c_str(), kDynHigh.toString().c_str());

  server.on("/", HTTP_GET, handleDashboard);
  server.on("/edit", HTTP_GET, handleEditForm);
  server.on("/api/reservations", HTTP_POST, handlePostReservation);
  server.on("/api/reservations/delete", HTTP_POST, handlePostDelete);
  server.onNotFound(handleDashboard);
  server.begin();
  Serial.printf("[ap-server] Web UI at http://%s/\n",
                WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
  dhcp::loop();

  // Heartbeat: how many stations are associated at the Wi-Fi layer (independent
  // of DHCP). Non-zero here + no DHCP RX means the client can't reach us on :67.
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    Serial.printf("[ap-server] stations associated: %d\n",
                  WiFi.softAPgetStationNum());
  }
}
