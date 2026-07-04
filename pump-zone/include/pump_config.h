#ifndef PUMP_CONFIG_H
#define PUMP_CONFIG_H
// -----------------------------------------------------------------------------
// pump-zone hardware / tunables — the shared contract the firmware #includes.
//
// These are COMMITTED, non-secret settings (GPIO map, HTTP port, relay polarity).
// Nothing here is sensitive: real WiFi credentials and the device id live in the
// gitignored include/secrets.h (see include/secrets.example.h).
//
// The macro NAMES below are a contract with the firmware source — do not rename.
// -----------------------------------------------------------------------------

#include "driver/gpio.h"  // for GPIO_NUM_* symbols

// --- Relay (pump) output ---
// GPIO2 matches the original esp-idf-iot/web-server wiring (relay IN on GPIO2).
// NOTE: GPIO2 is a boot strapping pin; with an active-low board it idles HIGH,
// which can block serial-download mode on some boards. It worked on the legacy
// hardware, so we keep it. If you rewire the relay, prefer a non-strapping pin
// (e.g. GPIO_NUM_23) and update this line.
#define RELAY_GPIO           GPIO_NUM_2    // relay IN pin (matches legacy wiring)
#define RELAY_ACTIVE_LEVEL   0             // 0 = active-low relay board (most common); 1 = active-high

// --- RGB status LED (LEDC PWM, active-high) ---
#define RGB_LED_GPIO_R       25
#define RGB_LED_GPIO_G       26
#define RGB_LED_GPIO_B       27
#define RGB_LED_ACTIVE_LOW   0

// --- HTTP server ---
#define PUMP_HTTP_PORT       80

#endif // PUMP_CONFIG_H
