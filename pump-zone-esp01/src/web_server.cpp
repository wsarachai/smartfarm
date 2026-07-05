#include "web_server.h"

#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <string.h>

#include "pump_config.h"  // PUMP_HTTP_PORT
#include "relay.h"

static ESP8266WebServer server(PUMP_HTTP_PORT);

// Emit {"relay_status":"ON|OFF"} (+ remaining_ms while running, + safety_off if the
// last off was the dead-man timer) — mirrors pump-zone plus the ESP-01's safety info.
static void send_status(int code) {
  const bool on = relay_get_state();

  JsonDocument doc;
  doc["relay_status"] = on ? "ON" : "OFF";
  if (on) {
    doc["remaining_ms"] = relay_remaining_ms();
  }
  if (relay_safety_tripped()) {
    doc["safety_off"] = true;  // pump was auto-cut, not turned off by a command
  }

  String out;
  serializeJson(doc, out);

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(code, "application/json", out);
}

static void send_error(int code, const char *message) {
  JsonDocument doc;
  doc["error"] = message;
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

// GET /api/v1/relay -> current pump state.
static void handle_relay_get() {
  Serial.println("[http] GET /api/v1/relay");
  send_status(200);
}

// POST /api/v1/relay  body: {"state":"on"|"off"} -> switch the pump.
static void handle_relay_post() {
  if (!server.hasArg("plain")) {
    Serial.println("[http] POST /api/v1/relay — empty body");
    send_error(400, "empty body; expected {\"state\":\"on\"|\"off\"}");
    return;
  }

  const String &body = server.arg("plain");
  Serial.printf("[http] POST /api/v1/relay body=%s\n", body.c_str());

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    send_error(400, "malformed JSON");
    return;
  }

  const char *state = doc["state"];
  if (state == nullptr) {
    send_error(400, "expected {\"state\":\"on\"|\"off\"}");
    return;
  }

  if (strcmp(state, "on") == 0) {
    relay_set(true);
  } else if (strcmp(state, "off") == 0) {
    relay_set(false);
  } else {
    send_error(400, "state must be \"on\" or \"off\"");
    return;
  }

  send_status(200);
}

void web_server_begin(void) {
  server.on("/api/v1/relay", HTTP_GET, handle_relay_get);
  server.on("/api/v1/relay", HTTP_POST, handle_relay_post);
  server.onNotFound([]() { send_error(404, "not found"); });
  server.begin();
  Serial.printf("[http] relay control server listening on :%d\n", PUMP_HTTP_PORT);
}

void web_server_loop(void) {
  server.handleClient();
}
