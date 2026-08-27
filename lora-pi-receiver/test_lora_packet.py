#!/usr/bin/env python3
"""
test_lora_packet.py — proves lora_packet.py stays byte-compatible with the
firmware's water-temp-node/src/lora/lora_packet.h.

The vectors below are NOT hand-written. They were produced by compiling that
very header on the host and calling its own packers, so a divergence between the
C and Python sides shows up here as a failing test rather than as a silently
wrong reading on the dashboard. Regenerate them the same way after any change to
the wire format:

    cat > /tmp/gen.c <<'EOF'
    #include <stdio.h>
    #include "lora_packet.h"
    int main(void) { ... pack and print hex ... }
    EOF
    gcc -std=c99 -I water-temp-node/src/lora /tmp/gen.c -o /tmp/gen && /tmp/gen

Run:  python3 -m unittest discover lora-pi-receiver
"""
import unittest

import lora_packet


# name -> (wire bytes as hex, expected decoded fields)
VECTORS = {
    # Every flag set, every field populated, all three versions of one payload.
    "v1_full": (
        "a1072aff102208d40c440062",
        dict(version=1, node_id=7, seq=42, flags=0xFF,
             temp_hot_c100=4130, temp_cold_c100=2260, battery_mv=3140,
             air_temp_c100=0, humidity_x100=0, pressure_dhpa=0, co2_ppm=0),
    ),
    "v2_full": (
        "a2072aff102208d40c44096d16bc2794005e",
        dict(version=2, node_id=7, seq=42, flags=0xFF,
             temp_hot_c100=4130, temp_cold_c100=2260, battery_mv=3140,
             air_temp_c100=2413, humidity_x100=5820, pressure_dhpa=10132,
             co2_ppm=0),
    ),
    "v3_full": (
        "a3072aff102208d40c44096d16bc2794032c00e3",
        dict(version=3, node_id=7, seq=42, flags=0xFF,
             temp_hot_c100=4130, temp_cold_c100=2260, battery_mv=3140,
             air_temp_c100=2413, humidity_x100=5820, pressure_dhpa=10132,
             co2_ppm=812),
    ),
    # Negative temperatures, a failed hot probe (sentinel), no CO2 yet.
    "v3_neg": (
        "a301ff1a8000f9f20000fe5727100000000000f7",
        dict(version=3, node_id=1, seq=255,
             flags=lora_packet.FLAG_COLD | lora_packet.FLAG_AIR | lora_packet.FLAG_HUM,
             temp_hot_c100=lora_packet.TEMP_INVALID, temp_cold_c100=-1550,
             battery_mv=0, air_temp_c100=-425, humidity_x100=10000,
             pressure_dhpa=0, co2_ppm=0),
    ),
    # u16 top end, to catch a signed/unsigned slip in battery or CO2.
    "v3_max": (
        "a3ff004400000000ffff0000000000009c40002a",
        dict(version=3, node_id=255, seq=0,
             flags=lora_packet.FLAG_BATT | lora_packet.FLAG_CO2,
             temp_hot_c100=0, temp_cold_c100=0, battery_mv=65535,
             air_temp_c100=0, humidity_x100=0, pressure_dhpa=0, co2_ppm=40000),
    ),
}


class TestDecode(unittest.TestCase):
    def test_firmware_vectors(self):
        for name, (hexstr, expected) in VECTORS.items():
            with self.subTest(vector=name):
                got = lora_packet.decode(bytes.fromhex(hexstr))
                self.assertIsNotNone(got, "%s failed to decode" % name)
                for key, want in expected.items():
                    self.assertEqual(got[key], want,
                                     "%s: %s was %r, expected %r"
                                     % (name, key, got[key], want))

    def test_every_version_decodes(self):
        seen = {lora_packet.decode(bytes.fromhex(h))["version"]
                for h, _ in VECTORS.values()}
        self.assertEqual(seen, {1, 2, 3})


class TestRejection(unittest.TestCase):
    """A frame the decoder cannot fully trust must be dropped, never guessed at."""

    GOOD = "a3072aff102208d40c44096d16bc2794032c00e3"

    def test_empty(self):
        self.assertIsNone(lora_packet.decode(b""))
        self.assertIsNone(lora_packet.decode(None))

    def test_bad_magic(self):
        bad = bytearray(bytes.fromhex(self.GOOD))
        bad[0] = 0xA9          # a version we do not speak
        self.assertIsNone(lora_packet.decode(bytes(bad)))

    def test_corrupt_crc(self):
        bad = bytearray(bytes.fromhex(self.GOOD))
        bad[-1] ^= 0x01
        self.assertIsNone(lora_packet.decode(bytes(bad)))

    def test_corrupt_payload_caught_by_crc(self):
        for i in range(lora_packet.PKT_LEN_V3 - 1):
            bad = bytearray(bytes.fromhex(self.GOOD))
            bad[i] ^= 0xFF
            with self.subTest(byte=i):
                self.assertIsNone(lora_packet.decode(bytes(bad)))

    def test_wrong_length_for_version(self):
        good = bytes.fromhex(self.GOOD)
        self.assertIsNone(lora_packet.decode(good[:-1]))       # truncated
        self.assertIsNone(lora_packet.decode(good + b"\x00"))  # trailing byte

    def test_v2_length_with_v3_magic(self):
        """The magic and the length have to agree — a v3 magic on an 18-byte
        frame is a corrupted v2, not a short v3."""
        v2 = bytearray(bytes.fromhex(VECTORS["v2_full"][0]))
        v2[0] = lora_packet.MAGIC_V3
        self.assertIsNone(lora_packet.decode(bytes(v2)))


class TestCrc8(unittest.TestCase):
    def test_matches_firmware_polynomial(self):
        # CRC-8/SMBUS over the ASCII digits "123456789" is 0xF4 — the standard
        # check value for poly 0x07 / init 0x00, which is what lora_crc8() is.
        self.assertEqual(lora_packet.crc8(b"123456789"), 0xF4)

    def test_all_zero_frame_property(self):
        # crc8 of all-zero bytes is 0, so a stuck-low bus produces a frame that
        # PASSES its own checksum. The magic byte is what actually rejects it.
        self.assertEqual(lora_packet.crc8(bytes(19)), 0)
        self.assertIsNone(lora_packet.decode(bytes(lora_packet.PKT_LEN_V3)))


if __name__ == "__main__":
    unittest.main()
