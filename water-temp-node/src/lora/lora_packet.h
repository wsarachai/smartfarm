/*
 * lora_packet.h — over-the-air binary frame, SHARED between water-temp-node and
 * lora-gateway. Big-endian on the wire. Keep this file identical in both
 * projects (copy in lora-gateway/src/lora/lora_packet.h).
 *
 * Five versions coexist; the gateway dispatches on the magic byte, so older
 * frames stay decodable forever. Each version APPENDS to the previous one — the
 * bytes a v1 frame defines mean the same thing in every later version.
 *
 * v1 (magic 0xA1, 12 bytes) — water temps + battery:
 *   0   magic/version   = LORA_PKT_MAGIC (0xA1), lets the gateway reject noise /
 *                         future-version frames cheaply
 *   1   node_id         1..255, maps to a friendly device_id in the gateway
 *   2   seq             rolling 0..255 counter, for de-dup + loss visibility
 *   3   flags           see LORA_FLAG_* below (which fields are valid)
 *   4-5 probe 0         int16 BE, centi-degC (41.30 C -> 4130). Sentinel if invalid.
 *   6-7 probe 1         int16 BE, centi-degC
 *   8-9 battery_mv      uint16 BE, millivolts of the 3V3 rail (== battery)
 *   10  reserved        0 (kept in CRC)
 *   11  crc8            CRC-8/SMBUS (poly 0x07, init 0x00) over bytes 0..10
 *
 * Bytes 4-7 were named `temp_hot` / `temp_cold` back when a node carried exactly
 * two probes. They are now simply **probe 0** and **probe 1** — same offsets,
 * same units, same flags. The gateway still labels them `temp_hot`/`temp_cold`
 * by default so dashboard history stays continuous (see gateway_config.h).
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

/*
 * v4 (magic 0xA4, 28 bytes) — the first 18 bytes of v3, then probes 2..5:
 *   18-19 probe 2    int16 BE, centi-degC
 *   20-21 probe 3
 *   22-23 probe 4
 *   24-25 probe 5
 *   26    reserved   0
 *   27    crc8       over bytes 0..26
 *
 * v4 is what a SIX-probe node sends. Probes 0 and 1 stay in their v1 slots, so
 * the two channels an existing install already plots keep their offsets, their
 * flags and their dashboard history.
 *
 * Note what v4 does NOT do: it adds no valid-flags. `flags` is FULL — all eight
 * bits are spoken for — so probes 2..5 signal "no reading" with the
 * LORA_TEMP_INVALID sentinel already defined for probes 0/1. A node with only
 * four probes fitted sends v4 with slots 4 and 5 set to the sentinel. Ask
 * lora_probe_valid() rather than testing either mechanism by hand.
 */
#define LORA_PKT_MAGIC_V4  0xA4
#define LORA_PKT_LEN_V4    28

/*
 * v5 (magic 0xA5, 36 bytes) — the first 26 bytes of v4, then air sensors 2 and 3:
 *   26-27 air_temp 1   int16 BE, centi-degC
 *   28-29 humidity 1   uint16 BE, %RH x100
 *   30-31 air_temp 2   int16 BE, centi-degC
 *   32-33 humidity 2   uint16 BE, %RH x100
 *   34    reserved     0
 *   35    crc8         over bytes 0..34
 *
 * v5 is what a node carrying THREE SHT45s sends. Air sensor 0 stays in the v2
 * slots (bytes 10-13) with its AIR/HUM flags, so an existing dashboard's
 * air_temp/humidity history is unbroken.
 *
 * Like v4's extra probes, sensors 1 and 2 carry validity as SENTINELS, because
 * `flags` has no spare bits: LORA_TEMP_INVALID for temperature and
 * LORA_HUM_INVALID for humidity. Ask lora_air_valid() / lora_hum_valid().
 */
#define LORA_PKT_MAGIC_V5  0xA5
#define LORA_PKT_LEN_V5    36

/* Longest frame any version can produce — size RX buffers with this. */
#define LORA_PKT_LEN_MAX   LORA_PKT_LEN_V5

/* Air temp/humidity sensors a v5 frame carries. v2/v3/v4 carry the first 1. */
#define LORA_AIR_MAX       3

/* Probe slots a v4 frame carries. v1/v2/v3 carry the first 2. */
#define LORA_PROBE_MAX     6

#define LORA_FLAG_HOT      0x01   /* probe 0 valid (historic name) */
#define LORA_FLAG_COLD     0x02   /* probe 1 valid (historic name) */
#define LORA_FLAG_BATT     0x04
#define LORA_FLAG_AIR      0x08   /* v2+: air_temp valid  */
#define LORA_FLAG_HUM      0x10   /* v2+: humidity valid  */
#define LORA_FLAG_PRESS    0x20   /* v2+: pressure valid  */
#define LORA_FLAG_CO2      0x40   /* v3:  co2_ppm valid   */
#define LORA_FLAG_SHT      0x80   /* v3:  air_temp/humidity came from the SHT4x
                                   *      (clear = from the BME280) */
/* No spare bits left. v4 and v5 both worked around this with sentinels; a
 * version that genuinely needs a new flag must add a second flags byte. */

/* Sentinel written into a temp field when that probe/sensor failed to read. */
#define LORA_TEMP_INVALID  ((int16_t)0x8000)

/* Same idea for humidity, whose valid range is 0..10000 (%RH x100). */
#define LORA_HUM_INVALID   ((uint16_t)0xFFFF)

typedef struct {
    uint8_t  version;         /* set by unpack (1..5); ignored by the packers   */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  probe_count;     /* set by unpack: 2 for v1-v3, LORA_PROBE_MAX for
                               * v4. Ignored by the packers — which version you
                               * send is decided by which packer you call.      */
    int16_t  probe_c100[LORA_PROBE_MAX];  /* centi-degC; [0]/[1] are the v1 slots */
    uint16_t battery_mv;
    uint8_t  air_count;       /* set by unpack: 1 for v2-v4, LORA_AIR_MAX for
                               * v5. Ignored by the packers.                   */
    /* v2+ ambient block; set the matching flags. Ignored by the v1 packer.
     * [0] is the v2 slot; [1]/[2] exist only in v5. */
    int16_t  air_temp_c100[LORA_AIR_MAX];   /* centi-degC */
    uint16_t humidity_x100[LORA_AIR_MAX];   /* %RH x100   */
    uint16_t pressure_dhpa;   /* hPa x10      */
    /* v3+; ignored by the v1/v2 packers. */
    uint16_t co2_ppm;         /* ppm          */
} lora_payload_t;

/*
 * Is probe i a number worth publishing? The two mechanisms differ by slot —
 * probes 0/1 have flag bits, probes 2..5 only the sentinel — so ask here rather
 * than testing either by hand. Also covers "this frame version is too old to
 * carry probe i at all".
 */
static inline int lora_probe_valid(const lora_payload_t *p, int i)
{
    if (i < 0 || i >= (int)p->probe_count) return 0;
    if (p->probe_c100[i] == LORA_TEMP_INVALID) return 0;
    if (i == 0) return (p->flags & LORA_FLAG_HOT)  != 0;
    if (i == 1) return (p->flags & LORA_FLAG_COLD) != 0;
    return 1;
}

/*
 * Same question for the air sensors. Sensor 0 gates on its flag bit, sensors 1
 * and 2 on the sentinel — v5 had no flag bits left to spend.
 */
static inline int lora_air_valid(const lora_payload_t *p, int i)
{
    if (i < 0 || i >= (int)p->air_count) return 0;
    if (i == 0) return (p->flags & LORA_FLAG_AIR) != 0;
    return p->air_temp_c100[i] != LORA_TEMP_INVALID;
}

static inline int lora_hum_valid(const lora_payload_t *p, int i)
{
    if (i < 0 || i >= (int)p->air_count) return 0;
    if (i == 0) return (p->flags & LORA_FLAG_HUM) != 0;
    return p->humidity_x100[i] != LORA_HUM_INVALID;
}

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
    out[4]  = (uint8_t)((uint16_t)p->probe_c100[0] >> 8);
    out[5]  = (uint8_t)((uint16_t)p->probe_c100[0] & 0xFF);
    out[6]  = (uint8_t)((uint16_t)p->probe_c100[1] >> 8);
    out[7]  = (uint8_t)((uint16_t)p->probe_c100[1] & 0xFF);
    out[8]  = (uint8_t)(p->battery_mv >> 8);
    out[9]  = (uint8_t)(p->battery_mv & 0xFF);
}

/* Bytes 10..15 (the v2 ambient block), shared by the v2, v3 and v4 packers. */
static inline void lora_packet_pack_ambient(const lora_payload_t *p, uint8_t *out)
{
    out[10] = (uint8_t)((uint16_t)p->air_temp_c100[0] >> 8);
    out[11] = (uint8_t)((uint16_t)p->air_temp_c100[0] & 0xFF);
    out[12] = (uint8_t)(p->humidity_x100[0] >> 8);
    out[13] = (uint8_t)(p->humidity_x100[0] & 0xFF);
    out[14] = (uint8_t)(p->pressure_dhpa >> 8);
    out[15] = (uint8_t)(p->pressure_dhpa & 0xFF);
}

/* Bytes 16..17 (CO2), shared by the v3 and v4 packers. */
static inline void lora_packet_pack_co2(const lora_payload_t *p, uint8_t *out)
{
    out[16] = (uint8_t)(p->co2_ppm >> 8);
    out[17] = (uint8_t)(p->co2_ppm & 0xFF);
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
    lora_packet_pack_co2(p, out);
    out[18] = 0x00;
    out[19] = lora_crc8(out, 19);
    return LORA_PKT_LEN_V3;
}

/* Bytes 18..25 (probes 2..5), shared by the v4 and v5 packers. */
static inline void lora_packet_pack_probes(const lora_payload_t *p, uint8_t *out)
{
    int i;
    for (i = 2; i < LORA_PROBE_MAX; i++) {
        out[18 + (i - 2) * 2] = (uint8_t)((uint16_t)p->probe_c100[i] >> 8);
        out[19 + (i - 2) * 2] = (uint8_t)((uint16_t)p->probe_c100[i] & 0xFF);
    }
}

/* Serialize p into a 28-byte v4 buffer (v3 fields + probes 2..5).
 * Returns LORA_PKT_LEN_V4. */
static inline int lora_packet_pack_v4(const lora_payload_t *p, uint8_t out[LORA_PKT_LEN_V4])
{
    out[0]  = LORA_PKT_MAGIC_V4;
    lora_packet_pack_common(p, out);
    lora_packet_pack_ambient(p, out);
    lora_packet_pack_co2(p, out);
    lora_packet_pack_probes(p, out);
    out[26] = 0x00;
    out[27] = lora_crc8(out, 27);
    return LORA_PKT_LEN_V4;
}

/* Serialize p into a 36-byte v5 buffer (v4 fields + air sensors 1 and 2).
 * Returns LORA_PKT_LEN_V5. */
static inline int lora_packet_pack_v5(const lora_payload_t *p, uint8_t out[LORA_PKT_LEN_V5])
{
    int i;
    out[0]  = LORA_PKT_MAGIC_V5;
    lora_packet_pack_common(p, out);
    lora_packet_pack_ambient(p, out);
    lora_packet_pack_co2(p, out);
    lora_packet_pack_probes(p, out);
    for (i = 1; i < LORA_AIR_MAX; i++) {
        out[26 + (i - 1) * 4] = (uint8_t)((uint16_t)p->air_temp_c100[i] >> 8);
        out[27 + (i - 1) * 4] = (uint8_t)((uint16_t)p->air_temp_c100[i] & 0xFF);
        out[28 + (i - 1) * 4] = (uint8_t)(p->humidity_x100[i] >> 8);
        out[29 + (i - 1) * 4] = (uint8_t)(p->humidity_x100[i] & 0xFF);
    }
    out[34] = 0x00;
    out[35] = lora_crc8(out, 35);
    return LORA_PKT_LEN_V5;
}

/*
 * Validate + parse a received buffer of ANY version into p. Fields the frame's
 * version does not carry are zeroed; probe slots it does not carry are set to
 * LORA_TEMP_INVALID and excluded by probe_count. p->version reports which
 * version it was. Returns 1 on success, 0 if length/magic/CRC are wrong.
 */
static inline int lora_packet_unpack(const uint8_t *in, size_t len, lora_payload_t *p)
{
    uint8_t ver;
    size_t  crc_at;
    int     i;

    if (len == 0) return 0;
    if      (in[0] == LORA_PKT_MAGIC    && len == LORA_PKT_LEN)    { ver = 1; crc_at = 11; }
    else if (in[0] == LORA_PKT_MAGIC_V2 && len == LORA_PKT_LEN_V2) { ver = 2; crc_at = 17; }
    else if (in[0] == LORA_PKT_MAGIC_V3 && len == LORA_PKT_LEN_V3) { ver = 3; crc_at = 19; }
    else if (in[0] == LORA_PKT_MAGIC_V4 && len == LORA_PKT_LEN_V4) { ver = 4; crc_at = 27; }
    else if (in[0] == LORA_PKT_MAGIC_V5 && len == LORA_PKT_LEN_V5) { ver = 5; crc_at = 35; }
    else return 0;

    if (lora_crc8(in, crc_at) != in[crc_at]) return 0;

    p->version        = ver;
    p->node_id        = in[1];
    p->seq            = in[2];
    p->flags          = in[3];
    p->probe_count    = (ver >= 4) ? LORA_PROBE_MAX : 2;
    p->probe_c100[0]  = (int16_t)(((uint16_t)in[4] << 8) | in[5]);
    p->probe_c100[1]  = (int16_t)(((uint16_t)in[6] << 8) | in[7]);
    for (i = 2; i < LORA_PROBE_MAX; i++) p->probe_c100[i] = LORA_TEMP_INVALID;
    p->battery_mv     = (uint16_t)(((uint16_t)in[8] << 8) | in[9]);
    p->air_count      = (ver >= 5) ? LORA_AIR_MAX : 1;
    for (i = 0; i < LORA_AIR_MAX; i++) {
        p->air_temp_c100[i] = LORA_TEMP_INVALID;
        p->humidity_x100[i] = LORA_HUM_INVALID;
    }
    p->air_temp_c100[0] = 0;
    p->humidity_x100[0] = 0;
    p->pressure_dhpa  = 0;
    p->co2_ppm        = 0;

    if (ver >= 2) {
        p->air_temp_c100[0] = (int16_t)(((uint16_t)in[10] << 8) | in[11]);
        p->humidity_x100[0] = (uint16_t)(((uint16_t)in[12] << 8) | in[13]);
        p->pressure_dhpa    = (uint16_t)(((uint16_t)in[14] << 8) | in[15]);
    }
    if (ver >= 3) {
        p->co2_ppm = (uint16_t)(((uint16_t)in[16] << 8) | in[17]);
    }
    if (ver >= 4) {
        for (i = 2; i < LORA_PROBE_MAX; i++) {
            p->probe_c100[i] = (int16_t)(((uint16_t)in[18 + (i - 2) * 2] << 8)
                                         | in[19 + (i - 2) * 2]);
        }
    }
    if (ver >= 5) {
        for (i = 1; i < LORA_AIR_MAX; i++) {
            p->air_temp_c100[i] = (int16_t)(((uint16_t)in[26 + (i - 1) * 4] << 8)
                                            | in[27 + (i - 1) * 4]);
            p->humidity_x100[i] = (uint16_t)(((uint16_t)in[28 + (i - 1) * 4] << 8)
                                             | in[29 + (i - 1) * 4]);
        }
    }
    return 1;
}

#endif /* LORA_PACKET_H */
