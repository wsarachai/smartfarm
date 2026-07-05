#ifndef RELAY_H
#define RELAY_H
// Relay (pump) driver + local safety cutoff.
//
// The relay is a direct GPIO0 drive. relay_set(true) also (re)arms a max-runtime
// dead-man timer (PUMP_MAX_RUN_MS); relay_loop() must be called frequently from
// loop() to enforce it. When the timer fires, the pump is forced OFF and
// relay_safety_tripped() latches true until the next explicit relay_set().

#include <stdbool.h>

void relay_init(void);                  // configures the pin and forces the pump OFF
void relay_set(bool on);                // switch pump; each ON re-arms the safety timer
bool relay_get_state(void);             // true = pump running
void relay_loop(void);                  // call from loop(): enforces the max-runtime cutoff
bool relay_safety_tripped(void);        // true if the LAST off was caused by the safety timer
unsigned long relay_remaining_ms(void); // ms until auto-off while running (0 when off/disabled)

#endif // RELAY_H
