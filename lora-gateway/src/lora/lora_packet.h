/*
 * lora_packet.h — over-the-air binary frame, SHARED between water-temp-node and
 * lora-gateway. Fixed 12 bytes, big-endian on the wire. Keep this file identical
 * in both projects (copy in lora-gateway/src/lora/lora_packet.h).
 *
 * Wire layout (offsets in bytes):
 *   0   magic/version   = LORA_PKT_MAGIC (0xA1) — proto v1, lets the gateway
 *                         reject noise / future-version frames cheaply
 *   1   node_id         1..255, maps to a friendly device_id in the gateway
 *   2   seq             rolling 0..255 counter, for de-dup + loss visibility
 *   3   flags           bit0 temp_hot valid, bit1 temp_cold valid, bit2 batt valid
 *   4-5 temp_hot        int16 BE, centi-degC (41.30 C -> 4130). Sentinel if invalid.
 *   6-7 temp_cold       int16 BE, centi-degC
 *   8-9 battery_mv      uint16 BE, millivolts of the 3V3 rail (== battery)
 *   10  reserved        0 (future use, kept in CRC)
 *   11  crc8            CRC-8/SMBUS (poly 0x07, init 0x00) over bytes 0..10
 */
#ifndef LORA_PACKET_H
#define LORA_PACKET_H

#include <stdint.h>
#include <stddef.h>

#define LORA_PKT_MAGIC     0xA1
#define LORA_PKT_LEN       12

#define LORA_FLAG_HOT      0x01
#define LORA_FLAG_COLD     0x02
#define LORA_FLAG_BATT     0x04

/* Sentinel written into a temp field when that probe failed to read. */
#define LORA_TEMP_INVALID  ((int16_t)0x8000)

typedef struct {
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    int16_t  temp_hot_c100;   /* centi-degC */
    int16_t  temp_cold_c100;  /* centi-degC */
    uint16_t battery_mv;
} lora_payload_t;

/* CRC-8/SMBUS (poly 0x07, init 0x00, no reflect). Used for the frame checksum. */
static inline uint8_t lora_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Serialize p into a LORA_PKT_LEN byte buffer. Returns LORA_PKT_LEN. */
static inline int lora_packet_pack(const lora_payload_t *p, uint8_t out[LORA_PKT_LEN])
{
    out[0]  = LORA_PKT_MAGIC;
    out[1]  = p->node_id;
    out[2]  = p->seq;
    out[3]  = p->flags;
    out[4]  = (uint8_t)((uint16_t)p->temp_hot_c100 >> 8);
    out[5]  = (uint8_t)((uint16_t)p->temp_hot_c100 & 0xFF);
    out[6]  = (uint8_t)((uint16_t)p->temp_cold_c100 >> 8);
    out[7]  = (uint8_t)((uint16_t)p->temp_cold_c100 & 0xFF);
    out[8]  = (uint8_t)(p->battery_mv >> 8);
    out[9]  = (uint8_t)(p->battery_mv & 0xFF);
    out[10] = 0x00;
    out[11] = lora_crc8(out, 11);
    return LORA_PKT_LEN;
}

/*
 * Validate + parse a received buffer into p.
 * Returns 1 on success, 0 if length/magic/CRC are wrong (drop the frame).
 */
static inline int lora_packet_unpack(const uint8_t *in, size_t len, lora_payload_t *p)
{
    if (len != LORA_PKT_LEN)          return 0;
    if (in[0] != LORA_PKT_MAGIC)      return 0;
    if (lora_crc8(in, 11) != in[11])  return 0;
    p->node_id        = in[1];
    p->seq            = in[2];
    p->flags          = in[3];
    p->temp_hot_c100  = (int16_t)(((uint16_t)in[4] << 8) | in[5]);
    p->temp_cold_c100 = (int16_t)(((uint16_t)in[6] << 8) | in[7]);
    p->battery_mv     = (uint16_t)(((uint16_t)in[8] << 8) | in[9]);
    return 1;
}

#endif /* LORA_PACKET_H */
