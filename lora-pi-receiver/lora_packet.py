"""
lora_packet.py — decode the 12-byte over-the-air frame from the sensor node.

Python port of water-temp-node/src/lora/lora_packet.h — MUST stay byte-compatible
with it. Wire layout (big-endian):
  0   magic (0xA1)      1   node_id       2   seq         3   flags
  4-5 temp_hot int16 centi-degC   6-7 temp_cold int16 centi-degC
  8-9 battery_mv uint16           10  reserved            11  crc8 (over 0..10)
"""

MAGIC = 0xA1
PKT_LEN = 12

FLAG_HOT = 0x01
FLAG_COLD = 0x02
FLAG_BATT = 0x04

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


def decode(buf):
    """Validate + parse a received frame. Returns a dict or None (drop the frame)."""
    if len(buf) != PKT_LEN:
        return None
    if buf[0] != MAGIC:
        return None
    if crc8(buf[:11]) != buf[11]:
        return None
    return {
        "node_id": buf[1],
        "seq": buf[2],
        "flags": buf[3],
        "temp_hot_c100": _i16(buf[4], buf[5]),
        "temp_cold_c100": _i16(buf[6], buf[7]),
        "battery_mv": (buf[8] << 8) | buf[9],
    }
