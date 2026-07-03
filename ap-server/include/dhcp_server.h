#pragma once
// -----------------------------------------------------------------------------
// Minimal custom DHCP server for the SoftAP.
//
// Replaces the built-in ESP DHCP server (which cannot do MAC->IP reservations).
// Reserved MACs (see reservations.h) always receive their fixed address in the
// .2 .. (poolFirst-1) band; every other client gets a dynamic lease from the
// poolFirst .. poolLast band. Polled from loop() on UDP port 67 — single
// threaded with the web server, so the reservation table needs no locking.
//
// Scope: DISCOVER/REQUEST/RELEASE/DECLINE/INFORM for a small SoftAP. Replies are
// broadcast to keep pre-IP clients simple. Not a general-purpose DHCP server.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <IPAddress.h>

namespace dhcp {

// Start the server. Call after softAP() and after the built-in DHCP server has
// been stopped. poolFirstOctet..poolLastOctet is the dynamic band (host octets).
void begin(const IPAddress &apIp, const IPAddress &netmask,
           uint8_t poolFirstOctet, uint8_t poolLastOctet, uint32_t leaseSecs);

// Poll for and service one or more DHCP packets. Call every loop().
void loop();

// Current IP for an associated MAC (reservation first, then dynamic lease).
// Returns false if we have no address for it.
bool ipForMac(const uint8_t mac[6], IPAddress &out);

// DHCP-reported hostname (option 12) for a dynamic client, or "" if unknown.
String hostnameForMac(const uint8_t mac[6]);

}  // namespace dhcp
