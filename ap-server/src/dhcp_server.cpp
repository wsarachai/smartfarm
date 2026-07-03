#include "dhcp_server.h"

#include <WiFiUdp.h>
#include <string.h>

#include "ap_config.h"
#include "reservations.h"

namespace dhcp {

// --- Wire constants ---------------------------------------------------------
static const uint16_t kServerPort = 67;
static const uint16_t kClientPort = 68;

static const uint8_t DHCP_DISCOVER = 1;
static const uint8_t DHCP_OFFER = 2;
static const uint8_t DHCP_REQUEST = 3;
static const uint8_t DHCP_DECLINE = 4;
static const uint8_t DHCP_ACK = 5;
static const uint8_t DHCP_NAK = 6;
static const uint8_t DHCP_RELEASE = 7;
static const uint8_t DHCP_INFORM = 8;

// BOOTP fixed header offsets (bytes), followed by the 4-byte magic cookie.
static const int OFS_OP = 0;
static const int OFS_XID = 4;
static const int OFS_FLAGS = 10;
static const int OFS_CIADDR = 12;
static const int OFS_YIADDR = 16;
static const int OFS_SIADDR = 20;
static const int OFS_GIADDR = 24;
static const int OFS_CHADDR = 28;
static const int OFS_MAGIC = 236;
static const int OFS_OPTIONS = 240;

static const uint8_t kMagic[4] = {0x63, 0x82, 0x53, 0x63};

static const char *msgName(uint8_t t) {
  switch (t) {
    case DHCP_DISCOVER: return "DISCOVER";
    case DHCP_OFFER: return "OFFER";
    case DHCP_REQUEST: return "REQUEST";
    case DHCP_DECLINE: return "DECLINE";
    case DHCP_ACK: return "ACK";
    case DHCP_NAK: return "NAK";
    case DHCP_RELEASE: return "RELEASE";
    case DHCP_INFORM: return "INFORM";
    default: return "?";
  }
}

// --- Module state -----------------------------------------------------------
struct DynLease {
  bool used;
  uint8_t mac[6];
  uint8_t octet;
  uint32_t expiryMs;
  char hostname[RESERVATION_LABEL_MAXLEN + 1];
};

// How long a tentatively-offered (pre-ACK) lease holds its slot, so two clients
// mid-handshake don't get offered the same address.
static const uint32_t kTentativeMs = 15000;

static WiFiUDP s_udp;
static uint8_t s_apBase[3];   // first three octets of the AP IP
static IPAddress s_apIp;
static IPAddress s_netmask;
static IPAddress s_broadcast;  // subnet-directed broadcast (apIp | ~netmask)
static uint8_t s_poolFirst;
static uint8_t s_poolLast;
static uint32_t s_leaseSecs;

static DynLease s_leases[AP_MAX_CONNECTIONS];
static int s_poolSize = 0;

static uint8_t s_buf[576];

// --- Helpers ----------------------------------------------------------------

static IPAddress ipFromOctet(uint8_t octet) {
  return IPAddress(s_apBase[0], s_apBase[1], s_apBase[2], octet);
}

static bool leaseExpired(const DynLease &l, uint32_t now) {
  return (int32_t)(now - l.expiryMs) >= 0;
}

// Finds a live dynamic lease for mac; returns index or -1.
static int findDynByMac(const uint8_t mac[6], uint32_t now) {
  for (int i = 0; i < s_poolSize; i++) {
    if (s_leases[i].used && !leaseExpired(s_leases[i], now) &&
        memcmp(s_leases[i].mac, mac, 6) == 0) {
      return i;
    }
  }
  return -1;
}

// Allocates (or reuses) a dynamic slot for mac; returns index or -1 if full.
static int allocDyn(const uint8_t mac[6], uint32_t now) {
  int existing = findDynByMac(mac, now);
  if (existing >= 0) {
    return existing;
  }
  // First free or expired slot.
  for (int i = 0; i < s_poolSize; i++) {
    if (!s_leases[i].used || leaseExpired(s_leases[i], now)) {
      memset(&s_leases[i], 0, sizeof(DynLease));
      s_leases[i].used = true;
      memcpy(s_leases[i].mac, mac, 6);
      s_leases[i].octet = (uint8_t)(s_poolFirst + i);
      s_leases[i].expiryMs = now + kTentativeMs;  // hold the slot until ACK
      return i;
    }
  }
  return -1;  // pool full
}

// Resolves the host octet this MAC should get. reserved=true if from reservation.
static bool octetForMac(const uint8_t mac[6], uint32_t now, uint8_t &octet,
                        bool &reserved, int &dynIndex) {
  const Reservation *r = reservations::findByMac(mac);
  if (r != nullptr) {
    octet = r->octet;
    reserved = true;
    dynIndex = -1;
    return true;
  }
  int idx = allocDyn(mac, now);
  if (idx < 0) {
    return false;
  }
  octet = s_leases[idx].octet;
  reserved = false;
  dynIndex = idx;
  return true;
}

// Scans the options area for option `code`; copies up to maxLen bytes into out,
// returns the option length (0 if not found). `len` is the total packet length.
static int getOption(int len, uint8_t code, uint8_t *out, int maxLen) {
  int i = OFS_OPTIONS;
  while (i + 1 < len) {
    uint8_t opt = s_buf[i];
    if (opt == 255) {
      break;  // end
    }
    if (opt == 0) {
      i++;  // pad
      continue;
    }
    uint8_t olen = s_buf[i + 1];
    if (i + 2 + olen > len) {
      break;
    }
    if (opt == code) {
      int n = olen < maxLen ? olen : maxLen;
      if (out) {
        memcpy(out, &s_buf[i + 2], n);
      }
      return olen;
    }
    i += 2 + olen;
  }
  return 0;
}

// Appends a TLV option to buf at *pos.
static void putOption(uint8_t *buf, int &pos, uint8_t code, const uint8_t *data,
                      uint8_t len) {
  buf[pos++] = code;
  buf[pos++] = len;
  memcpy(&buf[pos], data, len);
  pos += len;
}

static void put32(uint8_t *buf, int &pos, uint8_t code, uint32_t v) {
  uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8),
                  (uint8_t)v};
  putOption(buf, pos, code, b, 4);
}

static void putIp(uint8_t *buf, int &pos, uint8_t code, const IPAddress &ip) {
  uint8_t b[4] = {ip[0], ip[1], ip[2], ip[3]};
  putOption(buf, pos, code, b, 4);
}

// Builds and broadcasts a reply of the given message type with yiaddr.
static void sendReply(uint8_t msgType, const uint8_t *chaddr, uint32_t xidNet,
                      uint16_t flagsNet, const uint8_t *ciaddr,
                      const IPAddress &yiaddr) {
  uint8_t out[OFS_OPTIONS + 64];
  memset(out, 0, sizeof(out));

  out[OFS_OP] = 2;   // BOOTREPLY
  out[1] = 1;        // htype ethernet
  out[2] = 6;        // hlen
  memcpy(&out[OFS_XID], &xidNet, 4);
  memcpy(&out[OFS_FLAGS], &flagsNet, 2);
  if (msgType == DHCP_ACK && ciaddr) {
    memcpy(&out[OFS_CIADDR], ciaddr, 4);  // echo client's current addr on renew
  }
  if (msgType != DHCP_NAK) {
    out[OFS_YIADDR + 0] = yiaddr[0];
    out[OFS_YIADDR + 1] = yiaddr[1];
    out[OFS_YIADDR + 2] = yiaddr[2];
    out[OFS_YIADDR + 3] = yiaddr[3];
    out[OFS_SIADDR + 0] = s_apIp[0];
    out[OFS_SIADDR + 1] = s_apIp[1];
    out[OFS_SIADDR + 2] = s_apIp[2];
    out[OFS_SIADDR + 3] = s_apIp[3];
  }
  memcpy(&out[OFS_CHADDR], chaddr, 6);
  memcpy(&out[OFS_MAGIC], kMagic, 4);

  int pos = OFS_OPTIONS;
  uint8_t t = msgType;
  putOption(out, pos, 53, &t, 1);            // message type
  putIp(out, pos, 54, s_apIp);               // server identifier
  if (msgType != DHCP_NAK) {
    put32(out, pos, 51, s_leaseSecs);        // lease time
    putIp(out, pos, 1, s_netmask);           // subnet mask
    putIp(out, pos, 3, s_apIp);              // router
    putIp(out, pos, 6, s_apIp);              // DNS (AP itself; AP-only network)
  }
  out[pos++] = 255;                          // end

  // Subnet-directed broadcast (e.g. 192.168.1.255): reaches every associated
  // station at L2 with no ARP, and — unlike 255.255.255.255 — has a route via
  // the AP netif, so lwIP actually sends it.
  s_udp.beginPacket(s_broadcast, kClientPort);
  s_udp.write(out, pos);
  int sent = s_udp.endPacket();
  Serial.printf("[dhcp] TX %s yiaddr=%s (%d bytes, endPacket=%d)\n",
                msgName(msgType), yiaddr.toString().c_str(), pos, sent);
}

// --- Packet handling --------------------------------------------------------

static void handlePacket(int len) {
  if (len < OFS_OPTIONS + 1) {
    return;
  }
  if (memcmp(&s_buf[OFS_MAGIC], kMagic, 4) != 0) {
    return;  // not a DHCP packet
  }
  if (s_buf[OFS_OP] != 1) {
    return;  // not a BOOTREQUEST
  }

  uint8_t mac[6];
  memcpy(mac, &s_buf[OFS_CHADDR], 6);

  uint8_t typeBuf[1] = {0};
  if (getOption(len, 53, typeBuf, 1) == 0) {
    return;  // no message type
  }
  uint8_t msgType = typeBuf[0];
  Serial.printf("[dhcp] RX %s from %s (len=%d)\n", msgName(msgType),
                macToStr(mac).c_str(), len);

  uint32_t xidNet;
  memcpy(&xidNet, &s_buf[OFS_XID], 4);
  uint16_t flagsNet;
  memcpy(&flagsNet, &s_buf[OFS_FLAGS], 2);
  uint8_t ciaddr[4];
  memcpy(ciaddr, &s_buf[OFS_CIADDR], 4);

  // If the client is selecting a specific server, ignore unless it's us.
  uint8_t sid[4] = {0};
  if (getOption(len, 54, sid, 4) == 4) {
    IPAddress selected(sid[0], sid[1], sid[2], sid[3]);
    if (selected != s_apIp) {
      // Client picked another server; drop any tentative lease we made.
      if (msgType == DHCP_REQUEST) {
        int idx = findDynByMac(mac, millis());
        if (idx >= 0) {
          s_leases[idx].used = false;
        }
      }
      return;
    }
  }

  uint32_t now = millis();

  if (msgType == DHCP_RELEASE || msgType == DHCP_DECLINE) {
    int idx = findDynByMac(mac, now);
    if (idx >= 0) {
      s_leases[idx].used = false;
    }
    return;
  }

  if (msgType == DHCP_INFORM) {
    // Client already has an IP; just confirm options, no address.
    IPAddress none(ciaddr[0], ciaddr[1], ciaddr[2], ciaddr[3]);
    sendReply(DHCP_ACK, mac, xidNet, flagsNet, ciaddr, none);
    return;
  }

  if (msgType != DHCP_DISCOVER && msgType != DHCP_REQUEST) {
    return;
  }

  uint8_t octet;
  bool reserved;
  int dynIndex;
  if (!octetForMac(mac, now, octet, reserved, dynIndex)) {
    Serial.printf("[dhcp] no address available for %s (pool full?) — dropped\n",
                  macToStr(mac).c_str());
    return;
  }
  IPAddress offer = ipFromOctet(octet);
  Serial.printf("[dhcp] %s -> %s (%s)\n", macToStr(mac).c_str(),
                offer.toString().c_str(), reserved ? "reserved" : "dynamic");

  if (msgType == DHCP_DISCOVER) {
    sendReply(DHCP_OFFER, mac, xidNet, flagsNet, ciaddr, offer);
    return;
  }

  // DHCP_REQUEST: honour only if the requested address matches what we'd give.
  uint8_t req[4] = {0};
  IPAddress requested;
  if (getOption(len, 50, req, 4) == 4) {
    requested = IPAddress(req[0], req[1], req[2], req[3]);
  } else {
    requested = IPAddress(ciaddr[0], ciaddr[1], ciaddr[2], ciaddr[3]);
  }

  if (requested != offer) {
    sendReply(DHCP_NAK, mac, xidNet, flagsNet, ciaddr, offer);
    return;
  }

  // Commit: refresh dynamic lease expiry and capture hostname.
  if (!reserved && dynIndex >= 0) {
    s_leases[dynIndex].expiryMs = now + s_leaseSecs * 1000UL;
    uint8_t host[RESERVATION_LABEL_MAXLEN + 1] = {0};
    int hlen = getOption(len, 12, host, RESERVATION_LABEL_MAXLEN);
    if (hlen > 0) {
      memcpy(s_leases[dynIndex].hostname, host, hlen);
      s_leases[dynIndex].hostname[hlen] = '\0';
    }
  }
  sendReply(DHCP_ACK, mac, xidNet, flagsNet, ciaddr, offer);
}

// --- Public API -------------------------------------------------------------

void begin(const IPAddress &apIp, const IPAddress &netmask,
           uint8_t poolFirstOctet, uint8_t poolLastOctet, uint32_t leaseSecs) {
  s_apIp = apIp;
  s_netmask = netmask;
  s_broadcast = IPAddress(apIp[0] | (uint8_t)~netmask[0], apIp[1] | (uint8_t)~netmask[1],
                          apIp[2] | (uint8_t)~netmask[2], apIp[3] | (uint8_t)~netmask[3]);
  s_apBase[0] = apIp[0];
  s_apBase[1] = apIp[1];
  s_apBase[2] = apIp[2];
  s_poolFirst = poolFirstOctet;
  s_poolLast = poolLastOctet;
  s_leaseSecs = leaseSecs;

  s_poolSize = (int)(poolLastOctet - poolFirstOctet) + 1;
  if (s_poolSize > AP_MAX_CONNECTIONS) {
    s_poolSize = AP_MAX_CONNECTIONS;
  }
  memset(s_leases, 0, sizeof(s_leases));

  uint8_t ok = s_udp.begin(kServerPort);
  Serial.printf("[dhcp] %s UDP:%u  dynamic pool .%u-.%u  lease %us\n",
                ok ? "listening on" : "FAILED to bind", kServerPort,
                poolFirstOctet, poolLastOctet, (unsigned)leaseSecs);
}

void loop() {
  int len = s_udp.parsePacket();
  while (len > 0) {
    if (len > (int)sizeof(s_buf)) {
      len = sizeof(s_buf);
    }
    int n = s_udp.read(s_buf, len);
    if (n > 0) {
      handlePacket(n);
    }
    len = s_udp.parsePacket();
  }
}

bool ipForMac(const uint8_t mac[6], IPAddress &out) {
  const Reservation *r = reservations::findByMac(mac);
  if (r != nullptr) {
    out = ipFromOctet(r->octet);
    return true;
  }
  int idx = findDynByMac(mac, millis());
  if (idx >= 0) {
    out = ipFromOctet(s_leases[idx].octet);
    return true;
  }
  return false;
}

String hostnameForMac(const uint8_t mac[6]) {
  int idx = findDynByMac(mac, millis());
  if (idx >= 0 && s_leases[idx].hostname[0] != '\0') {
    return String(s_leases[idx].hostname);
  }
  return String("");
}

}  // namespace dhcp
