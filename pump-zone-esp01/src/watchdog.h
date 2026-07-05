#ifndef WATCHDOG_H
#define WATCHDOG_H
// Device watchdog for the pump node — recover automatically when the device has
// a problem, in two layers:
//
//   1. Acute-hang watchdog — arms the ESP8266 software watchdog and feeds it each
//      loop. If loop() stops feeding it (a wedged HTTP handler, a blocked call),
//      the chip resets itself. The always-on hardware watchdog (~8 s) backstops it.
//
//   2. Lost-link recovery — a pump that can't reach the AP can no longer be
//      commanded (it can't receive an OFF), which is unsafe. If WiFi stays down
//      continuously past WIFI_RECOVER_MS, force the pump OFF and reboot to recover
//      the link/DHCP. Complements the dead-man cutoff (which bounds pump-ON time)
//      and WiFi.setAutoReconnect() (which handles brief blips).

#include <stdbool.h>

void watchdog_init(void);                 // arm the hardware/loop watchdog
void watchdog_loop(bool wifi_connected);  // feed it + run lost-link recovery

#endif  // WATCHDOG_H
