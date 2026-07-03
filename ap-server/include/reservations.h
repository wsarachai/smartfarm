#pragma once
// -----------------------------------------------------------------------------
// MAC -> IP reservation store, persisted in ESP32 NVS (Arduino Preferences).
//
// Reservations pin a device (by MAC) to a fixed host address in the reserved
// band .2 .. (DHCP_POOL_FIRST_HOST - 1). The custom DHCP server consults this
// table; the web UI edits it. The whole table is saved to NVS as one blob and
// reloaded at boot, so it survives reboots.
// -----------------------------------------------------------------------------

#include <Arduino.h>

#include "ap_config.h"

struct Reservation {
  uint8_t mac[6];
  uint8_t octet;                              // host octet, in the reserved band
  char label[RESERVATION_LABEL_MAXLEN + 1];   // optional friendly name (NUL-term)
};

// Result of an upsert attempt (also used to surface validation errors in the UI).
enum ResvResult {
  RESV_OK = 0,
  RESV_FULL,        // table at MAX_RESERVATIONS
  RESV_BAD_MAC,     // unparseable / all-zero / broadcast MAC
  RESV_BAD_IP,      // octet outside the reserved band
  RESV_DUP_IP,      // that IP is already reserved for a different MAC
};

namespace reservations {

// Loads the table from NVS. Call once in setup(), before the DHCP server starts.
void begin();

int count();
const Reservation *all();                     // pointer to count() contiguous entries

// Returns the entry for this MAC, or nullptr.
const Reservation *findByMac(const uint8_t mac[6]);

// True if some entry (other than exceptMac, which may be nullptr) uses octet.
bool octetTaken(uint8_t octet, const uint8_t *exceptMac);

// Lowest unused host octet in the reserved band, or 0 if the band is full.
uint8_t nextFreeOctet();

// Insert or update (by MAC). Validates MAC and octet. Persists on success.
ResvResult upsert(const uint8_t mac[6], uint8_t octet, const char *label);

// Removes the entry for this MAC (if any) and persists. Returns true if removed.
bool removeByMac(const uint8_t mac[6]);

// Human-readable message for a ResvResult (for the UI error banner).
const char *resultMessage(ResvResult r);

}  // namespace reservations

// --- MAC helpers (shared with the DHCP server and web UI) ---
// Parses "AA:BB:CC:DD:EE:FF" or "aa-bb-...", case-insensitive, ':' or '-' seps.
bool parseMac(const char *str, uint8_t out[6]);
String macToStr(const uint8_t mac[6]);
