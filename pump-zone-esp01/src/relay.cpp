#include "relay.h"

#include <Arduino.h>

#include "pump_config.h"  // RELAY_GPIO, RELAY_ACTIVE_LEVEL, PUMP_MAX_RUN_MS

// Resolve the active/inactive digital levels from the configured polarity.
#define RELAY_ON_LEVEL  (RELAY_ACTIVE_LEVEL ? HIGH : LOW)
#define RELAY_OFF_LEVEL (RELAY_ACTIVE_LEVEL ? LOW : HIGH)

static bool s_on = false;
static bool s_safety_tripped = false;
static unsigned long s_deadline = 0;  // millis() at which auto-off fires (valid while s_on)

static void relay_write(bool on) {
  digitalWrite(RELAY_GPIO, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

void relay_init(void) {
  pinMode(RELAY_GPIO, OUTPUT);
  relay_write(false);  // force pump OFF first thing (see GPIO0 boot note in pump_config.h)
  s_on = false;
  s_safety_tripped = false;
  Serial.printf("[relay] init on GPIO%d (active-%s), pump OFF\n",
                RELAY_GPIO, RELAY_ACTIVE_LEVEL ? "high" : "low");
}

void relay_set(bool on) {
  relay_write(on);
  s_on = on;
  s_safety_tripped = false;  // an explicit command clears any prior safety latch
  if (on) {
#if PUMP_MAX_RUN_MS > 0
    s_deadline = millis() + PUMP_MAX_RUN_MS;  // (re)arm the dead-man timer on each ON
    Serial.printf("[relay] pump ON (auto-off in %lu ms)\n", (unsigned long)PUMP_MAX_RUN_MS);
#else
    Serial.println("[relay] pump ON (safety cutoff disabled)");
#endif
  } else {
    Serial.println("[relay] pump OFF");
  }
}

bool relay_get_state(void) {
  return s_on;
}

bool relay_safety_tripped(void) {
  return s_safety_tripped;
}

unsigned long relay_remaining_ms(void) {
#if PUMP_MAX_RUN_MS > 0
  if (!s_on) {
    return 0;
  }
  // Signed delta tolerates millis() wraparound (~49 days).
  long rem = (long)(s_deadline - millis());
  return rem > 0 ? (unsigned long)rem : 0;
#else
  return 0;
#endif
}

void relay_loop(void) {
#if PUMP_MAX_RUN_MS > 0
  if (s_on && (long)(millis() - s_deadline) >= 0) {
    relay_write(false);
    s_on = false;
    s_safety_tripped = true;
    Serial.println("[relay] SAFETY CUTOFF — max runtime reached, pump forced OFF");
  }
#endif
}
