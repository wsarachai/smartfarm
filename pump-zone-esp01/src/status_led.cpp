#include "status_led.h"

#include <Arduino.h>

#include "pump_config.h"  // STATUS_LED_GPIO, STATUS_LED_ACTIVE_LOW

#define LED_ON_LEVEL  (STATUS_LED_ACTIVE_LOW ? LOW : HIGH)
#define LED_OFF_LEVEL (STATUS_LED_ACTIVE_LOW ? HIGH : LOW)

static led_status_t s_status = LED_CONNECTING;

static void led_write(bool on) {
  digitalWrite(STATUS_LED_GPIO, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

void status_led_init(void) {
  pinMode(STATUS_LED_GPIO, OUTPUT);
  led_write(false);
}

void status_led_set(led_status_t status) {
  s_status = status;
}

void status_led_loop(void) {
  const unsigned long now = millis();
  bool on;

  switch (s_status) {
    case LED_READY:
      on = true;  // solid
      break;

    case LED_CONNECTING:
      on = (now % 200UL) < 100UL;  // ~5 Hz fast blink
      break;

    case LED_WIFI_LOST:
      on = (now % 1500UL) < 150UL;  // brief blip every 1.5 s
      break;

    case LED_PUMP_RUNNING: {
      const unsigned long p = now % 1000UL;  // heartbeat double-blink
      on = (p < 100UL) || (p >= 200UL && p < 300UL);
      break;
    }

    default:
      on = false;
      break;
  }

  led_write(on);
}
