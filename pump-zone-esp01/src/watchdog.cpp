#include "watchdog.h"

#include <Arduino.h>

#include "pump_config.h"  // WDT_TIMEOUT_MS, WIFI_RECOVER_MS
#include "relay.h"        // relay_set — force the pump OFF before a recovery reboot

// millis() when the WiFi link first dropped; 0 means "currently connected".
static unsigned long s_wifi_lost_since = 0;

void watchdog_init(void) {
  // Arm the software watchdog with an explicit window; the ESP8266's hardware
  // watchdog (always on, ~8 s) backstops it if even this stops being serviced.
  // loop() must call watchdog_loop() often enough to feed it.
  ESP.wdtDisable();
  ESP.wdtEnable(WDT_TIMEOUT_MS);
  Serial.printf("[wdt] watchdog armed (%lu ms window)\n", (unsigned long)WDT_TIMEOUT_MS);
}

void watchdog_loop(bool wifi_connected) {
  ESP.wdtFeed();  // acute-hang guard: a wedged loop stops feeding -> chip resets

#if WIFI_RECOVER_MS > 0
  if (wifi_connected) {
    s_wifi_lost_since = 0;  // link healthy — reset the recovery timer
    return;
  }

  const unsigned long now = millis();
  if (s_wifi_lost_since == 0) {
    s_wifi_lost_since = now ? now : 1;  // start the clock (avoid the 0 sentinel at t=0)
    return;
  }

  // Signed delta tolerates millis() wraparound (~49 days).
  if ((long)(now - s_wifi_lost_since) >= (long)WIFI_RECOVER_MS) {
    // Up but uncommandable for too long: make the pump safe, then reboot to
    // recover connectivity/DHCP. A crashed hub is already covered by the
    // dead-man cutoff; this covers a wedged *local* link.
    Serial.println("[wdt] WiFi down too long — forcing pump OFF and rebooting");
    relay_set(false);
    delay(50);  // let the relay settle + serial flush
    ESP.restart();
  }
#endif
}
