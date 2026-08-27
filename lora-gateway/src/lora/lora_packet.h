/*
 * lora_packet.h — over-the-air binary frame, SHARED between water-temp-node and
 * lora-gateway. Big-endian on the wire. Keep this file identical in both
 * projects (copy in lora-gateway/src/lora/lora_packet.h).
 *
 * Three versions coexist; the gateway dispatches on the magic byte, so older
 * frames stay decodable forever. Each version APPENDS to the previous one — the
 * bytes a v1 frame defines mean the same thing in v2 and v3.
 *
 * v1 (magic 0xA1, 12 bytes) — water temps + battery:
 *   0   magic/version   = LORA_PKT_MAGIC (0xA1), lets the gateway reject noise /
 *                         future-version frames cheaply
 *   1   node_id         1..255, maps to a friendly device_id in the gateway
 *   2   seq             rolling 0..255 counter, for de-dup + loss visibility
 *   3   flags           see LORA_FLAG_* below (which fields are valid)
 *   4-5 temp_hot        int16 BE, centi-degC (41.30 C -> 4130). Sentinel if invalid.
 *   6-7 temp_cold       int16 BE, centi-degC
 *   8-9 battery_mv      uint16 BE, millivolts of the 3V3 rail (== battery)
 *   10  reserved        0 (kept in CRC)
 *   11  crc8            CRC-8/SMBUS (poly 0x07, init 0x00) over bytes 0..10
 */
#ifndef LORA_PACKET_H
#define LORA_PACKET_H

#include <stdint.h>
#include <stddef.h>

#define LORA_PKT_MAGIC     0xA1
#define LORA_PKT_LEN       12

/*
 * v2 (magic 0xA2, 18 bytes) — the first 10 bytes of v1, then an ambient block:
 *   10-11 air_temp   int16 BE, centi-degC
 *   12-13 humidity   uint16 BE, %RH x100 (0..10000)
 *   14-15 pressure   uint16 BE, hPa x10 (deci-hPa, e.g. 1013.2 hPa -> 10132)
 *   16    reserved   0
 *   17    crc8       over bytes 0..16
 */
#define LORA_PKT_MAGIC_V2  0xA2
#define LORA_PKT_LEN_V2    18

/*
 * v3 (magic 0xA3, 20 bytes) — the first 16 bytes of v2, then CO2:
 *   16-17 co2_ppm    uint16 BE, ppm (SCD41; 400..5000 typical, 0 = not measured)
 *   18    reserved   0
 *   19    crc8       over bytes 0..18
 *
 * v3 is what a node carrying the SHT45 + SCD41 sends. Bytes 10-13 (air_temp and
 * humidity) then come from the SHT45 rather than the BME280, and LORA_FLAG_SHT
 * is set to say so; a BME280, where fitted, contributes only `pressure`. The
 * receiver need not care — the units and offsets are unchanged either way.
 */
#define LORA_PKT_MAGIC_V3  0xA3
#define LORA_PKT_LEN_V3    20

/* Longest frame any version can produce — size RX buffers with this. */
#define LORA_PKT_LEN_MAX   LORA_PKT_LEN_V3

#define LORA_FLAG_HOT      0x01
#define LORA_FLAG_COLD     0x02
#define LORA_FLAG_BATT     0x04
#define LORA_FLAG_AIR      0x08   /* v2+: air_temp valid  */
#define LORA_FLAG_HUM      0x10   /* v2+: humidity valid  */
#define LORA_FLAG_PRESS    0x20   /* v2+: pressure valid  */
#define LORA_FLAG_CO2      0x40   /* v3:  co2_ppm valid   */
#define LORA_FLAG_SHT      0x80   /* v3:  air_temp/humidity came from the SHT4x
                                   *      (clear = from the BME280) */

/* Sentinel written into a temp field when that probe failed to read. */
#define LORA_TEMP_INVALID  ((int16_t)0x8000)

typedef struct {
    uint8_t  version;         /* set by unpack (1/2/3); ignored by the packers */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    int16_t  temp_hot_c100;   /* centi-degC */
    int16_t  temp_cold_c100;  /* centi-degC */
    uint16_t battery_mv;
    /* v2+ ambient block; set the matching flags. Ignored by the v1 packer. */
    int16_t  air_temp_c100;   /* centi-degC   */
    uint16_t humidity_x100;   /* %RH x100     */
    uint16_t pressure_dhpa;   /* hPa x10      */
    /* v3; ignored by the v1/v2 packers. */
    uint16_t co2_ppm;         /* ppm          */
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

/* Bytes 1..9 are identical in every version; fill them once. */
static inline void lora_packet_pack_common(const lora_payload_t *p, uint8_t *out)
{
    out[1]  = p->node_id;
    out[2]  = p->seq;
    out[3]  = p->flags;
    out[4]  = (uint8_t)((uint16_t)p->temp_hot_c100 >> 8);
    out[5]  = (uint8_t)((uint16_t)p->temp_hot_c100 & 0xFF);
    out[6]  = (uint8_t)((uint16_t)p->temp_cold_c100 >> 8);
    out[7]  = (uint8_t)((uint16_t)p->temp_cold_c100 & 0xFF);
    out[8]  = (uint8_t)(p->battery_mv >> 8);
    out[9]  = (uint8_t)(p->battery_mv & 0xFF);
}

/* Bytes 10..15 (the v2 ambient block), shared by the v2 and v3 packers. */
static inline void lora_packet_pack_ambient(const lora_payload_t *p, uint8_t *out)
{
    out[10] = (uint8_t)((uint16_t)p->air_temp_c100 >> 8);
    out[11] = (uint8_t)((uint16_t)p->air_temp_c100 & 0xFF);
    out[12] = (uint8_t)(p->humidity_x100 >> 8);
    out[13] = (uint8_t)(p->humidity_x100 & 0xFF);
    out[14] = (uint8_t)(p->pressure_dhpa >> 8);
    out[15] = (uint8_t)(p->pressure_dhpa & 0xFF);
}

/* Serialize p into a LORA_PKT_LEN byte buffer. Returns LORA_PKT_LEN. */
static inline int lora_packet_pack(const lora_payload_t *p, uint8_t out[LORA_PKT_LEN])
{
    out[0]  = LORA_PKT_MAGIC;
    lora_packet_pack_common(p, out);
    out[10] = 0x00;
    out[11] = lora_crc8(out, 11);
    return LORA_PKT_LEN;
}

/* Serialize p into an 18-byte v2 buffer (v1 fields + ambient). Returns LORA_PKT_LEN_V2. */
static inline int lora_packet_pack_v2(const lora_payload_t *p, uint8_t out[LORA_PKT_LEN_V2])
{
    out[0]  = LORA_PKT_MAGIC_V2;
    lora_packet_pack_common(p, out);
    lora_packet_pack_ambient(p, out);
    out[16] = 0x00;
    out[17] = lora_crc8(out, 17);
    return LORA_PKT_LEN_V2;
}

/* Serialize p into a 20-byte v3 buffer (v2 fields + CO2). Returns LORA_PKT_LEN_V3. */
static inline int lora_packet_pack_v3(const lora_payload_t *p, uint8_t out[LORA_PKT_LEN_V3])
{
    out[0]  = LORA_PKT_MAGIC_V3;
    lora_packet_pack_common(p, out);
    lora_packet_pack_ambient(p, out);
    out[16] = (uint8_t)(p->co2_ppm >> 8);
    out[17] = (uint8_t)(p->co2_ppm & 0xFF);
    out[18] = 0x00;
    out[19] = lora_crc8(out, 19);
    return LORA_PKT_LEN_V3;
}

/*
 * Validate + parse a received buffer of ANY version into p. Fields the frame's
 * version does not carry are zeroed, and p->version reports which version it was.
 * Returns 1 on success, 0 if length/magic/CRC are wrong (drop the frame).
 */
static inline int lora_packet_unpack(const uint8_t *in, size_t len, lora_payload_t *p)
{
    uint8_t ver;
    size_t  crc_at;

    if (len == 0) return 0;
    if      (in[0] == LORA_PKT_MAGIC    && len == LORA_PKT_LEN)    { ver = 1; crc_at = 11; }
    else if (in[0] == LORA_PKT_MAGIC_V2 && len == LORA_PKT_LEN_V2) { ver = 2; crc_at = 17; }
    else if (in[0] == LORA_PKT_MAGIC_V3 && len == LORA_PKT_LEN_V3) { ver = 3; crc_at = 19; }
    else return 0;

    if (lora_crc8(in, crc_at) != in[crc_at]) return 0;

    p->version        = ver;
    p->node_id        = in[1];
    p->seq            = in[2];
    p->flags          = in[3];
    p->temp_hot_c100  = (int16_t)(((uint16_t)in[4] << 8) | in[5]);
    p->temp_cold_c100 = (int16_t)(((uint16_t)in[6] << 8) | in[7]);
    p->battery_mv     = (uint16_t)(((uint16_t)in[8] << 8) | in[9]);
    p->air_temp_c100  = 0;
    p->humidity_x100  = 0;
    p->pressure_dhpa  = 0;
    p->co2_ppm        = 0;

    if (ver >= 2) {
        p->air_temp_c100 = (int16_t)(((uint16_t)in[10] << 8) | in[11]);
        p->humidity_x100 = (uint16_t)(((uint16_t)in[12] << 8) | in[13]);
        p->pressure_dhpa = (uint16_t)(((uint16_t)in[14] << 8) | in[15]);
    }
    if (ver >= 3) {
        p->co2_ppm = (uint16_t)(((uint16_t)in[16] << 8) | in[17]);
    }
    return 1;
}

#endif /* LORA_PACKET_H */
