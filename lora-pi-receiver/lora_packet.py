"""
lora_packet.py — decode the over-the-air frame from the sensor node.

Python port of water-temp-node/src/lora/lora_packet.h — MUST stay byte-compatible.
Supports all three frame versions (dispatch on the magic byte); each version
appends to the previous one, so the offsets an older frame defines never move.

v1 (magic 0xA1, 12 bytes):
  0 magic  1 node_id  2 seq  3 flags
  4-5 temp_hot i16 c-degC   6-7 temp_cold i16 c-degC
  8-9 battery_mv u16        10 reserved   11 crc8 (0..10)

v2 (magic 0xA2, 18 bytes) — v1's first 10 bytes + an ambient block:
  10-11 air_temp i16 c-degC   12-13 humidity u16 %RH x100
  14-15 pressure u16 hPa x10  16 reserved   17 crc8 (0..16)

v3 (magic 0xA3, 20 bytes) — v2's first 16 bytes + CO2:
  16-17 co2 u16 ppm           18 reserved   19 crc8 (0..18)

In a v3 frame, air_temp/humidity come from the SHT45 rather than the BME280
when FLAG_SHT is set — same units, same offsets, better accuracy.
"""

MAGIC_V1 = 0xA1
PKT_LEN_V1 = 12
MAGIC_V2 = 0xA2
PKT_LEN_V2 = 18
MAGIC_V3 = 0xA3
PKT_LEN_V3 = 20

# Longest frame any version can produce — size RX buffers with this.
PKT_LEN_MAX = PKT_LEN_V3

FLAG_HOT = 0x01
FLAG_COLD = 0x02
FLAG_BATT = 0x04
FLAG_AIR = 0x08
FLAG_HUM = 0x10
FLAG_PRESS = 0x20
FLAG_CO2 = 0x40
FLAG_SHT = 0x80  # air_temp/humidity came from the SHT45, not the BME280

TEMP_INVALID = -32768  # int16 0x8000

# magic -> (version, expected length, index of the CRC byte)
_VERSIONS = {
    MAGIC_V1: (1, PKT_LEN_V1, 11),
    MAGIC_V2: (2, PKT_LEN_V2, 17),
    MAGIC_V3: (3, PKT_LEN_V3, 19),
}


def crc8(data):
    """CRC-8/SMBUS (poly 0x07, init 0x00) — matches lora_crc8() in the firmware."""
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc


def _i16(hi, lo):
    v = (hi << 8) | lo
    return v - 0x10000 if v & 0x8000 else v


def _u16(hi, lo):
    return (hi << 8) | lo


def decode(buf):
    """Validate + parse a received frame of any version. Returns a dict (with all
    keys present, fields the version does not carry set to 0) or None (drop it)."""
    if not buf:
        return None

    ver_info = _VERSIONS.get(buf[0])
    if ver_info is None:
        return None
    version, expect_len, crc_at = ver_info
    if len(buf) != expect_len:
        return None
    if crc8(buf[:crc_at]) != buf[crc_at]:
        return None

    d = {
        "version": version,
        "node_id": buf[1],
        "seq": buf[2],
        "flags": buf[3],
        "temp_hot_c100": _i16(buf[4], buf[5]),
        "temp_cold_c100": _i16(buf[6], buf[7]),
        "battery_mv": _u16(buf[8], buf[9]),
        "air_temp_c100": 0,
        "humidity_x100": 0,
        "pressure_dhpa": 0,
        "co2_ppm": 0,
    }
    if version >= 2:
        d["air_temp_c100"] = _i16(buf[10], buf[11])
        d["humidity_x100"] = _u16(buf[12], buf[13])
        d["pressure_dhpa"] = _u16(buf[14], buf[15])
    if version >= 3:
        d["co2_ppm"] = _u16(buf[16], buf[17])
    return d
