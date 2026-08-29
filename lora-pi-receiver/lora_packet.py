"""
lora_packet.py — decode the over-the-air frame from the sensor node.

Python port of water-temp-node/src/lora/lora_packet.h — MUST stay byte-compatible.
Supports all five frame versions (dispatch on the magic byte); each version
appends to the previous one, so the offsets an older frame defines never move.

v1 (magic 0xA1, 12 bytes):
  0 magic  1 node_id  2 seq  3 flags
  4-5 probe 0 i16 c-degC    6-7 probe 1 i16 c-degC
  8-9 battery_mv u16        10 reserved   11 crc8 (0..10)

v2 (magic 0xA2, 18 bytes) — v1's first 10 bytes + an ambient block:
  10-11 air_temp i16 c-degC   12-13 humidity u16 %RH x100
  14-15 pressure u16 hPa x10  16 reserved   17 crc8 (0..16)

v3 (magic 0xA3, 20 bytes) — v2's first 16 bytes + CO2:
  16-17 co2 u16 ppm           18 reserved   19 crc8 (0..18)

v4 (magic 0xA4, 28 bytes) — v3's first 18 bytes + probes 2..5:
  18-19 probe 2  20-21 probe 3  22-23 probe 4  24-25 probe 5
  26 reserved   27 crc8 (0..26)

v5 (magic 0xA5, 36 bytes) — v4's first 26 bytes + air sensors 1 and 2:
  26-27 air_temp 1  28-29 humidity 1
  30-31 air_temp 2  32-33 humidity 2
  34 reserved   35 crc8 (0..34)

A v5 node carries THREE SHT45s. They share one factory-fixed I2C address, so
on the hardware side they sit behind a bus switch — but that is invisible
here. Air sensor 0 stays in the v2 slots, so existing air_temp/humidity
history is unbroken; sensors 1 and 2 carry validity as sentinels, because the
flags byte is full.

Bytes 4-7 were called temp_hot / temp_cold when a node had exactly two probes.
They are now probe 0 and probe 1 — same offsets, same units, same flags — and
receiver.py still publishes them under the old metric names so dashboard history
stays continuous.

In a v3+ frame, air_temp/humidity come from the SHT45 rather than the BME280
when FLAG_SHT is set — same units, same offsets, better accuracy.
"""

MAGIC_V1 = 0xA1
PKT_LEN_V1 = 12
MAGIC_V2 = 0xA2
PKT_LEN_V2 = 18
MAGIC_V3 = 0xA3
PKT_LEN_V3 = 20
MAGIC_V4 = 0xA4
PKT_LEN_V4 = 28
MAGIC_V5 = 0xA5
PKT_LEN_V5 = 36

# Longest frame any version can produce — size RX buffers with this.
PKT_LEN_MAX = PKT_LEN_V5

# Probe slots a v4 frame carries. v1/v2/v3 carry the first 2.
PROBE_MAX = 6

# Air temp/humidity sensors a v5 frame carries. v2/v3/v4 carry the first 1.
AIR_MAX = 3

FLAG_HOT = 0x01  # probe 0 valid (historic name)
FLAG_COLD = 0x02  # probe 1 valid (historic name)
FLAG_BATT = 0x04
FLAG_AIR = 0x08
FLAG_HUM = 0x10
FLAG_PRESS = 0x20
FLAG_CO2 = 0x40
FLAG_SHT = 0x80  # air_temp/humidity came from the SHT45, not the BME280
# No spare bits: that is why probes 2..5 carry validity as TEMP_INVALID instead.

TEMP_INVALID = -32768  # int16 0x8000
HUM_INVALID = 0xFFFF   # humidity's valid range is 0..10000

# magic -> (version, expected length, index of the CRC byte)
_VERSIONS = {
    MAGIC_V1: (1, PKT_LEN_V1, 11),
    MAGIC_V2: (2, PKT_LEN_V2, 17),
    MAGIC_V3: (3, PKT_LEN_V3, 19),
    MAGIC_V4: (4, PKT_LEN_V4, 27),
    MAGIC_V5: (5, PKT_LEN_V5, 35),
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


def probe_valid(pkt, i):
    """Is probe i a number worth publishing? Port of lora_probe_valid().

    The two mechanisms differ by slot — probes 0/1 have flag bits, probes 2..5
    only the sentinel — so ask here rather than testing either by hand. Also
    covers 'this frame version is too old to carry probe i at all'.
    """
    if i < 0 or i >= pkt["probe_count"]:
        return False
    if pkt["probes"][i] == TEMP_INVALID:
        return False
    if i == 0:
        return bool(pkt["flags"] & FLAG_HOT)
    if i == 1:
        return bool(pkt["flags"] & FLAG_COLD)
    return True


def air_valid(pkt, i):
    """Is air sensor i's TEMPERATURE worth publishing? Port of lora_air_valid().

    Sensor 0 gates on FLAG_AIR, sensors 1/2 on the sentinel — v5 had no flag
    bits left to spend.
    """
    if i < 0 or i >= pkt["air_count"]:
        return False
    if i == 0:
        return bool(pkt["flags"] & FLAG_AIR)
    return pkt["air_temp_c100"][i] != TEMP_INVALID


def hum_valid(pkt, i):
    """Same for humidity. Port of lora_hum_valid()."""
    if i < 0 or i >= pkt["air_count"]:
        return False
    if i == 0:
        return bool(pkt["flags"] & FLAG_HUM)
    return pkt["humidity_x100"][i] != HUM_INVALID


def decode(buf):
    """Validate + parse a received frame of any version. Returns a dict (with all
    keys present, fields the version does not carry set to 0, probe slots it does
    not carry set to TEMP_INVALID) or None (drop it)."""
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

    probes = [TEMP_INVALID] * PROBE_MAX
    probes[0] = _i16(buf[4], buf[5])
    probes[1] = _i16(buf[6], buf[7])

    d = {
        "version": version,
        "node_id": buf[1],
        "seq": buf[2],
        "flags": buf[3],
        "probe_count": PROBE_MAX if version >= 4 else 2,
        "probes": probes,
        "battery_mv": _u16(buf[8], buf[9]),
        "air_count": AIR_MAX if version >= 5 else 1,
        "air_temp_c100": [0] + [TEMP_INVALID] * (AIR_MAX - 1),
        "humidity_x100": [0] + [HUM_INVALID] * (AIR_MAX - 1),
        "pressure_dhpa": 0,
        "co2_ppm": 0,
    }
    if version >= 2:
        d["air_temp_c100"][0] = _i16(buf[10], buf[11])
        d["humidity_x100"][0] = _u16(buf[12], buf[13])
        d["pressure_dhpa"] = _u16(buf[14], buf[15])
    if version >= 3:
        d["co2_ppm"] = _u16(buf[16], buf[17])
    if version >= 4:
        for i in range(2, PROBE_MAX):
            off = 18 + (i - 2) * 2
            probes[i] = _i16(buf[off], buf[off + 1])
    if version >= 5:
        for i in range(1, AIR_MAX):
            off = 26 + (i - 1) * 4
            d["air_temp_c100"][i] = _i16(buf[off], buf[off + 1])
            d["humidity_x100"][i] = _u16(buf[off + 2], buf[off + 3])
    return d
