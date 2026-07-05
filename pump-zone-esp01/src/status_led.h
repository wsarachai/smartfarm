#ifndef STATUS_LED_H
#define STATUS_LED_H
// Single onboard LED (GPIO2, active-low) driven with non-blocking blink codes.
// The ESP-01 has no room for the RGB LED the ESP32 pump-zone used, so the four
// connectivity colors collapse into distinct blink patterns on one LED.
//
//   LED_CONNECTING   fast blink   (booting / joining WiFi, server not up)
//   LED_READY        solid on     (connected + HTTP server running)
//   LED_WIFI_LOST    slow blip    (was up, WiFi dropped)
//   LED_PUMP_RUNNING double-blink  (pump energized — overrides connectivity)

typedef enum {
  LED_CONNECTING = 0,
  LED_READY,
  LED_WIFI_LOST,
  LED_PUMP_RUNNING,
} led_status_t;

void status_led_init(void);
void status_led_set(led_status_t status);  // idempotent; safe to call every loop
void status_led_loop(void);                // call frequently: drives the blink timing

#endif // STATUS_LED_H
