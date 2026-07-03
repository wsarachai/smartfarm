#include "reservations.h"

#include <Preferences.h>
#include <string.h>

// Reserved band is .2 .. (DHCP_POOL_FIRST_HOST - 1).
static const uint8_t kReservedLow = 2;
static const uint8_t kReservedHigh = DHCP_POOL_FIRST_HOST - 1;

static Reservation s_table[MAX_RESERVATIONS];
static int s_count = 0;

static const char *kNvsNamespace = "apres";
static const char *kNvsBlobKey = "res";
static const char *kNvsCountKey = "n";

// --- MAC helpers ------------------------------------------------------------

bool parseMac(const char *str, uint8_t out[6]) {
  if (str == nullptr) {
    return false;
  }
  uint8_t bytes[6];
  int nibbles = 0;
  uint8_t cur = 0;
  for (const char *p = str; *p; ++p) {
    char c = *p;
    if (c == ':' || c == '-' || c == ' ') {
      continue;
    }
    uint8_t v;
    if (c >= '0' && c <= '9') {
      v = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    } else {
      return false;  // non-hex, non-separator
    }
    cur = (uint8_t)((cur << 4) | v);
    if (++nibbles % 2 == 0) {
      int idx = nibbles / 2 - 1;
      if (idx >= 6) {
        return false;  // too many bytes
      }
      bytes[idx] = cur;
      cur = 0;
    }
  }
  if (nibbles != 12) {
    return false;
  }
  memcpy(out, bytes, 6);
  return true;
}

String macToStr(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

static bool macIsAllZero(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) {
      return false;
    }
  }
  return true;
}

static bool macIsBroadcast(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) {
      return false;
    }
  }
  return true;
}

// --- Store ------------------------------------------------------------------

namespace reservations {

static void persist() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);
  prefs.putUInt(kNvsCountKey, (uint32_t)s_count);
  prefs.putBytes(kNvsBlobKey, s_table, sizeof(Reservation) * s_count);
  prefs.end();
}

void begin() {
  Preferences prefs;
  prefs.begin(kNvsNamespace, true);  // read-only
  uint32_t n = prefs.getUInt(kNvsCountKey, 0);
  if (n > MAX_RESERVATIONS) {
    n = MAX_RESERVATIONS;
  }
  size_t want = sizeof(Reservation) * n;
  size_t got = prefs.getBytes(kNvsBlobKey, s_table, want);
  prefs.end();

  if (got == want && want > 0) {
    s_count = (int)n;
  } else {
    s_count = 0;  // nothing stored yet, or size mismatch after a struct change
  }
}

int count() { return s_count; }

const Reservation *all() { return s_table; }

const Reservation *findByMac(const uint8_t mac[6]) {
  for (int i = 0; i < s_count; i++) {
    if (memcmp(s_table[i].mac, mac, 6) == 0) {
      return &s_table[i];
    }
  }
  return nullptr;
}

bool octetTaken(uint8_t octet, const uint8_t *exceptMac) {
  for (int i = 0; i < s_count; i++) {
    if (exceptMac && memcmp(s_table[i].mac, exceptMac, 6) == 0) {
      continue;
    }
    if (s_table[i].octet == octet) {
      return true;
    }
  }
  return false;
}

uint8_t nextFreeOctet() {
  for (uint8_t o = kReservedLow; o <= kReservedHigh; o++) {
    if (!octetTaken(o, nullptr)) {
      return o;
    }
  }
  return 0;  // band full
}

ResvResult upsert(const uint8_t mac[6], uint8_t octet, const char *label) {
  if (macIsAllZero(mac) || macIsBroadcast(mac) || (mac[0] & 0x01)) {
    return RESV_BAD_MAC;  // reject zero, broadcast, and multicast MACs
  }
  if (octet < kReservedLow || octet > kReservedHigh) {
    return RESV_BAD_IP;
  }
  if (octetTaken(octet, mac)) {
    return RESV_DUP_IP;
  }

  Reservation *slot = nullptr;
  for (int i = 0; i < s_count; i++) {
    if (memcmp(s_table[i].mac, mac, 6) == 0) {
      slot = &s_table[i];
      break;
    }
  }
  if (slot == nullptr) {
    if (s_count >= MAX_RESERVATIONS) {
      return RESV_FULL;
    }
    slot = &s_table[s_count++];
    memcpy(slot->mac, mac, 6);
  }

  slot->octet = octet;
  memset(slot->label, 0, sizeof(slot->label));
  if (label) {
    strncpy(slot->label, label, RESERVATION_LABEL_MAXLEN);
    slot->label[RESERVATION_LABEL_MAXLEN] = '\0';
  }

  persist();
  return RESV_OK;
}

bool removeByMac(const uint8_t mac[6]) {
  for (int i = 0; i < s_count; i++) {
    if (memcmp(s_table[i].mac, mac, 6) == 0) {
      // Compact by moving the last entry into the hole.
      if (i != s_count - 1) {
        s_table[i] = s_table[s_count - 1];
      }
      s_count--;
      persist();
      return true;
    }
  }
  return false;
}

const char *resultMessage(ResvResult r) {
  switch (r) {
    case RESV_OK:
      return "Saved.";
    case RESV_FULL:
      return "Reservation table is full.";
    case RESV_BAD_MAC:
      return "Invalid MAC address.";
    case RESV_BAD_IP:
      return "IP is outside the reserved band (.2-.99).";
    case RESV_DUP_IP:
      return "That IP is already reserved for another device.";
    default:
      return "Unknown error.";
  }
}

}  // namespace reservations
