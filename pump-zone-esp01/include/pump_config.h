#ifndef PUMP_CONFIG_H
#define PUMP_CONFIG_H
// -----------------------------------------------------------------------------
// pump-zone-esp01 hardware / tunables — the committed, non-secret contract.
//
// GPIO map, relay polarity, HTTP port, and the safety cutoff live here. Real
// WiFi credentials, the device id, and the OTA password live in the gitignored
// include/secrets.h (see include/secrets.example.h).
//
// ESP-01/01S pin reality: only GPIO0 and GPIO2 are broken out for use, and BOTH
// are boot-mode straps that must idle HIGH at power-on. TX/RX (GPIO1/3) carry
// the serial console. That is the entire budget — hence one relay + one LED.
// -----------------------------------------------------------------------------

// --- Relay (pump) output ---
// GPIO0 is the only pin free for a load. It is ALSO the flash-mode strap: it
// must idle HIGH to boot normally; pulling it LOW enters serial-download mode.
// Because it idles HIGH before firmware runs, an active-HIGH board may click the
// pump momentarily at power-on — unavoidable in software. relay_init() forces
// the pump OFF as its first action.
#define RELAY_GPIO            0
#define RELAY_ACTIVE_LEVEL    1   // 1 = active-high (most ESP-01S relay boards); 0 = active-low

// --- Status LED ---
// Onboard blue LED on GPIO2, active-low. GPIO2 is also a boot strap (must idle
// HIGH); active-low means the LED sits OFF at boot, which satisfies the strap.
#define STATUS_LED_GPIO       2
#define STATUS_LED_ACTIVE_LOW 1

// --- HTTP server ---
#define PUMP_HTTP_PORT        80

// --- Static IP ---
// Claim a fixed address instead of relying on a DHCP reservation, so the hub can
// target this node directly and it survives an ap-server reservation-table wipe.
// .5 sits inside ap-server's reserved .2–.99 range and OUTSIDE its .100–.109
// dynamic pool, so there is no lease conflict as long as no ap-server reservation
// also claims .5. Set USE_STATIC_IP to 0 to fall back to DHCP.
#define USE_STATIC_IP         1
#define STATIC_IP_ADDR        192, 168, 0, 5     // this node's fixed address
#define STATIC_GATEWAY        192, 168, 0, 1     // the ap-server AP
#define STATIC_SUBNET         255, 255, 255, 0
#define STATIC_DNS            192, 168, 0, 1     // AP handles DNS (or set 0,0,0,0)

// --- Safety cutoff (dead-man timer) ---
// The pump auto-shuts OFF this many ms after the last POST {"state":"on"} unless
// refreshed by another {"state":"on"} (each 'on' re-arms the countdown). This is
// the local node's protection against a silent/crashed hub leaving the pump on.
// Set to 0 to DISABLE (not recommended for a real pump).
#define PUMP_MAX_RUN_MS       300000UL   // 5 minutes

#endif // PUMP_CONFIG_H
