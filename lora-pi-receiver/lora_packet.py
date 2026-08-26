"""
lora_packet.py — decode the over-the-air frame from the sensor node.

Python port of water-temp-node/src/lora/lora_packet.h — MUST stay byte-compatible.
Supports both frame versions (dispatch on the magic byte):

v1 (magic 0xA1, 12 bytes):
  0 magic  1 node_id  2 seq  3 flags
  4-5 temp_hot i16 c-degC   6-7 temp_cold i16 c-degC
  8-9 battery_mv u16        10 reserved   11 crc8 (0..10)

v2 (magic 0xA2, 18 bytes) — v1 first 10 bytes + a BME280 block:
  10-11 air_temp i16 c-degC   12-13 humidity u16 %RH x100
  14-15 pressure u16 hPa x10  16 reserved   17 crc8 (0..16)
"""

MAGIC_V1 = 0xA1
PKT_LEN_V1 = 12
MAGIC_V2 = 0xA2
PKT_LEN_V2 = 18

FLAG_HOT = 0x01
FLAG_COLD = 0x02
FLAG_BATT = 0x04
FLAG_AIR = 0x08
FLAG_HUM = 0x10
FLAG_PRESS = 0x20

TEMP_INVALID = -32768  # int16 0x8000


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
    """Validate + parse a received frame. Returns a dict (with all keys present,
    v2 fields 0 for a v1 frame) or None (drop the frame)."""
    if not buf:
        return None
    magic = buf[0]

    if magic == MAGIC_V1 and len(buf) == PKT_LEN_V1:
        if crc8(buf[:11]) != buf[11]:
            return None
        end = 11
    elif magic == MAGIC_V2 and len(buf) == PKT_LEN_V2:
        if crc8(buf[:17]) != buf[17]:
            return None
        end = 17
    else:
        return None

    d = {
        "version": 1 if magic == MAGIC_V1 else 2,
        "node_id": buf[1],
        "seq": buf[2],
        "flags": buf[3],
        "temp_hot_c100": _i16(buf[4], buf[5]),
        "temp_cold_c100": _i16(buf[6], buf[7]),
        "battery_mv": _u16(buf[8], buf[9]),
        "air_temp_c100": 0,
        "humidity_x100": 0,
        "pressure_dhpa": 0,
    }
    if magic == MAGIC_V2:
        d["air_temp_c100"] = _i16(buf[10], buf[11])
        d["humidity_x100"] = _u16(buf[12], buf[13])
        d["pressure_dhpa"] = _u16(buf[14], buf[15])
    return d
