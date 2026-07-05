// pump-zone-esp01 — ESP-01/01S irrigation-pump relay node.
//
// Drop-in replacement for the ESP32 ../pump-zone controller: same DEVICE_ID and
// same /api/v1/relay contract, rewritten on the Arduino/ESP8266 stack. Joins the
// AP as a STA, serves the relay control endpoint, enforces a local safety cutoff,
// blinks status on the onboard LED, and accepts OTA updates.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

#include "pump_config.h"
#include "relay.h"
#include "secrets.h"  // DEVICE_ID, OTA_PASSWORD
#include "status_led.h"
#include "watchdog.h"
#include "web_server.h"
#include "wifi.h"

static bool s_services_up = false;   // web server + OTA started
static bool s_was_connected = false;

static void ota_begin(void) {
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    // Safety: never leave the pump running across a firmware flash.
    relay_set(false);
    Serial.println("[ota] update starting — pump forced OFF");
  });
  ArduinoOTA.onEnd([]() { Serial.println("[ota] update complete"); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ota] error %u\n", error);
  });

  ArduinoOTA.begin();
  Serial.printf("[ota] ready as \"%s\"\n", DEVICE_ID);
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("\n[main] pump-zone-esp01 \"%s\" booting\n", DEVICE_ID);

  relay_init();  // forces the pump OFF first thing
  status_led_init();
  status_led_set(LED_CONNECTING);

  wifi_begin();
  watchdog_init();  // arm the device-hang watchdog once boot init is done
  // The HTTP server + OTA come up once WiFi connects (handled in loop()).
}

void loop() {
  relay_loop();       // enforce the max-runtime safety cutoff
  status_led_loop();  // drive blink timing

  const bool connected = wifi_is_connected();
  watchdog_loop(connected);  // feed the watchdog + recover from a wedged link

  // Start services once, on the first successful connection.
  if (connected && !s_was_connected) {
    Serial.println("[wifi] connected");
    Serial.printf("[wifi]   IP:      %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[wifi]   gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[wifi]   subnet:  %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("[wifi]   RSSI:    %d dBm\n", WiFi.RSSI());
    Serial.printf("[http]   POST/GET http://%s/api/v1/relay\n", WiFi.localIP().toString().c_str());
    if (!s_services_up) {
      web_server_begin();
      ota_begin();
      s_services_up = true;
    }
  }
  s_was_connected = connected;

  if (s_services_up) {
    web_server_loop();
    ArduinoOTA.handle();
  }

  // LED priority: a running pump overrides connectivity status.
  if (relay_get_state()) {
    status_led_set(LED_PUMP_RUNNING);
  } else if (connected) {
    status_led_set(LED_READY);
  } else {
    status_led_set(s_services_up ? LED_WIFI_LOST : LED_CONNECTING);
  }
}
