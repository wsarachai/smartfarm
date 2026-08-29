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

The v1/v2/v3 vectors are unchanged from before v4 and v5 existed, and the v4 ones
from before v5 — that is the point of keeping them here. Each version appends
without moving a single earlier byte.

Run:  python3 -m unittest discover lora-pi-receiver
"""
import unittest

import lora_packet


INV = lora_packet.TEMP_INVALID
HINV = lora_packet.HUM_INVALID

# An air block with only sensor 0 populated — what every pre-v5 frame decodes to.
ONE_AIR = dict(air_count=1)


def air(t0=0, h0=0, t1=INV, h1=HINV, t2=INV, h2=HINV):
    return dict(air_temp_c100=[t0, t1, t2], humidity_x100=[h0, h1, h2])


# name -> (wire bytes as hex, expected decoded fields)
VECTORS = {
    # Every flag set, every field populated, all five versions of one payload.
    "v1_full": (
        "a1072aff102208d40c440062",
        dict(version=1, node_id=7, seq=42, flags=0xFF, probe_count=2, air_count=1,
             probes=[4130, 2260, INV, INV, INV, INV],
             battery_mv=3140, pressure_dhpa=0, co2_ppm=0, **air()),
    ),
    "v2_full": (
        "a2072aff102208d40c44096d16bc2794005e",
        dict(version=2, node_id=7, seq=42, flags=0xFF, probe_count=2, air_count=1,
             probes=[4130, 2260, INV, INV, INV, INV],
             battery_mv=3140, pressure_dhpa=10132, co2_ppm=0,
             **air(2413, 5820)),
    ),
    "v3_full": (
        "a3072aff102208d40c44096d16bc2794032c00e3",
        dict(version=3, node_id=7, seq=42, flags=0xFF, probe_count=2, air_count=1,
             probes=[4130, 2260, INV, INV, INV, INV],
             battery_mv=3140, pressure_dhpa=10132, co2_ppm=812,
             **air(2413, 5820)),
    ),
    "v4_full": (
        "a4072aff102208d40c44096d16bc2794032c09060928fbe621340032",
        dict(version=4, node_id=7, seq=42, flags=0xFF, probe_count=6, air_count=1,
             probes=[4130, 2260, 2310, 2344, -1050, 8500],
             battery_mv=3140, pressure_dhpa=10132, co2_ppm=812,
             **air(2413, 5820)),
    ),
    # Six probes AND three air sensors — the full v5 payload.
    "v5_full": (
        "a5072aff102208d40c44096d16bc2794032c09060928fbe6213409c5190afeb62710001e",
        dict(version=5, node_id=7, seq=42, flags=0xFF, probe_count=6, air_count=3,
             probes=[4130, 2260, 2310, 2344, -1050, 8500],
             battery_mv=3140, pressure_dhpa=10132, co2_ppm=812,
             air_temp_c100=[2413, 2501, -330],
             humidity_x100=[5820, 6410, 10000]),
    ),
    # Negative temperatures, a failed probe 0 (sentinel), no CO2 yet.
    "v3_neg": (
        "a301ff1a8000f9f20000fe5727100000000000f7",
        dict(version=3, node_id=1, seq=255,
             flags=lora_packet.FLAG_COLD | lora_packet.FLAG_AIR | lora_packet.FLAG_HUM,
             probe_count=2, air_count=1,
             probes=[INV, -1550, INV, INV, INV, INV],
             battery_mv=0, pressure_dhpa=0, co2_ppm=0,
             **air(-425, 10000)),
    ),
    # A six-probe node with only FOUR probes plugged in: slots 4/5 are sentinel.
    "v4_partial": (
        "a402090704d2fdc90ce400000000000000000000270f800080000092",
        dict(version=4, node_id=2, seq=9,
             flags=lora_packet.FLAG_HOT | lora_packet.FLAG_COLD | lora_packet.FLAG_BATT,
             probe_count=6, air_count=1,
             probes=[1234, -567, 0, 9999, INV, INV],
             battery_mv=3300, pressure_dhpa=0, co2_ppm=0, **air()),
    ),
    # Probe 0 failed while 2..5 read fine — the flag bits and the sentinel are
    # independent mechanisms, and a slot-0 failure must not suppress the rest.
    "v4_p0_failed": (
        "a4036402800007d0000000000000000000000834089808fc09600046",
        dict(version=4, node_id=3, seq=100, flags=lora_packet.FLAG_COLD,
             probe_count=6, air_count=1,
             probes=[INV, 2000, 2100, 2200, 2300, 2400],
             battery_mv=0, pressure_dhpa=0, co2_ppm=0, **air()),
    ),
    # A three-SHT45 node with only ONE sensor answering: 1 and 2 are sentinels.
    # This is the air-side mirror of v4_partial.
    "v5_one_sht": (
        "a5044d98800080000000089813880000000080008000800080008000ffff8000ffff001c",
        dict(version=5, node_id=4, seq=77,
             flags=lora_packet.FLAG_AIR | lora_packet.FLAG_HUM | lora_packet.FLAG_SHT,
             probe_count=6, air_count=3,
             probes=[INV, INV, INV, INV, INV, INV],
             battery_mv=0, pressure_dhpa=0, co2_ppm=0,
             **air(2200, 5000)),
    ),
    # u16 top end, to catch a signed/unsigned slip in battery or CO2.
    "v3_max": (
        "a3ff004400000000ffff0000000000009c40002a",
        dict(version=3, node_id=255, seq=0,
             flags=lora_packet.FLAG_BATT | lora_packet.FLAG_CO2,
             probe_count=2, air_count=1,
             probes=[0, 0, INV, INV, INV, INV],
             battery_mv=65535, pressure_dhpa=0, co2_ppm=40000, **air()),
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
        self.assertEqual(seen, {1, 2, 3, 4, 5})

    def test_each_version_appends_without_moving_earlier_bytes(self):
        """The whole point of the version scheme. If this ever stops holding,
        every older node in the field decodes as garbage."""
        v3 = bytes.fromhex(VECTORS["v3_full"][0])
        v4 = bytes.fromhex(VECTORS["v4_full"][0])
        v5 = bytes.fromhex(VECTORS["v5_full"][0])
        # byte 0 is the magic, which is what differs by design
        self.assertEqual(v3[1:18], v4[1:18])
        self.assertEqual(v4[1:26], v5[1:26])


class TestProbeValid(unittest.TestCase):
    """probe_valid() is the only sanctioned way to ask 'publish this probe?' —
    probes 0/1 gate on a flag bit, probes 2..5 on the sentinel."""

    def valid_set(self, name):
        pkt = lora_packet.decode(bytes.fromhex(VECTORS[name][0]))
        return {i for i in range(lora_packet.PROBE_MAX)
                if lora_packet.probe_valid(pkt, i)}

    def test_old_frame_carries_only_two_probes(self):
        self.assertEqual(self.valid_set("v3_full"), {0, 1})

    def test_v4_all_six(self):
        self.assertEqual(self.valid_set("v4_full"), {0, 1, 2, 3, 4, 5})

    def test_unfitted_probes_excluded(self):
        self.assertEqual(self.valid_set("v4_partial"), {0, 1, 2, 3})

    def test_slot0_failure_does_not_suppress_the_others(self):
        self.assertEqual(self.valid_set("v4_p0_failed"), {1, 2, 3, 4, 5})

    def test_flag_clear_beats_a_plausible_value(self):
        pkt = lora_packet.decode(bytes.fromhex(VECTORS["v4_full"][0]))
        pkt["flags"] &= ~lora_packet.FLAG_COLD
        self.assertFalse(lora_packet.probe_valid(pkt, 1))
        self.assertTrue(lora_packet.probe_valid(pkt, 2))

    def test_out_of_range_index(self):
        pkt = lora_packet.decode(bytes.fromhex(VECTORS["v4_full"][0]))
        self.assertFalse(lora_packet.probe_valid(pkt, -1))
        self.assertFalse(lora_packet.probe_valid(pkt, lora_packet.PROBE_MAX))


class TestAirValid(unittest.TestCase):
    """Same contract on the air side: sensor 0 gates on FLAG_AIR / FLAG_HUM,
    sensors 1 and 2 on their sentinels."""

    def sets(self, name):
        pkt = lora_packet.decode(bytes.fromhex(VECTORS[name][0]))
        return ({i for i in range(lora_packet.AIR_MAX) if lora_packet.air_valid(pkt, i)},
                {i for i in range(lora_packet.AIR_MAX) if lora_packet.hum_valid(pkt, i)})

    def test_pre_v5_frame_carries_only_one_sensor(self):
        # v4_full sets every flag, but air_count is 1 — sensors 1/2 do not exist.
        self.assertEqual(self.sets("v4_full"), ({0}, {0}))

    def test_v5_all_three(self):
        self.assertEqual(self.sets("v5_full"), ({0, 1, 2}, {0, 1, 2}))

    def test_unfitted_air_sensors_excluded(self):
        self.assertEqual(self.sets("v5_one_sht"), ({0}, {0}))

    def test_flag_clear_suppresses_only_sensor_0(self):
        pkt = lora_packet.decode(bytes.fromhex(VECTORS["v5_full"][0]))
        pkt["flags"] &= ~lora_packet.FLAG_AIR
        self.assertFalse(lora_packet.air_valid(pkt, 0))
        self.assertTrue(lora_packet.air_valid(pkt, 1))
        # humidity has its own flag and must be unaffected
        self.assertTrue(lora_packet.hum_valid(pkt, 0))

    def test_humidity_sentinel_is_distinct_from_zero(self):
        """0 %RH is a legal reading; only 0xFFFF means 'no sensor'."""
        pkt = lora_packet.decode(bytes.fromhex(VECTORS["v5_full"][0]))
        pkt["humidity_x100"][1] = 0
        self.assertTrue(lora_packet.hum_valid(pkt, 1))
        pkt["humidity_x100"][1] = lora_packet.HUM_INVALID
        self.assertFalse(lora_packet.hum_valid(pkt, 1))

    def test_out_of_range_index(self):
        pkt = lora_packet.decode(bytes.fromhex(VECTORS["v5_full"][0]))
        for f in (lora_packet.air_valid, lora_packet.hum_valid):
            self.assertFalse(f(pkt, -1))
            self.assertFalse(f(pkt, lora_packet.AIR_MAX))


class TestRejection(unittest.TestCase):
    """A frame the decoder cannot fully trust must be dropped, never guessed at."""

    GOOD = "a5072aff102208d40c44096d16bc2794032c09060928fbe6213409c5190afeb62710001e"

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
        for i in range(lora_packet.PKT_LEN_V5 - 1):
            bad = bytearray(bytes.fromhex(self.GOOD))
            bad[i] ^= 0xFF
            with self.subTest(byte=i):
                self.assertIsNone(lora_packet.decode(bytes(bad)))

    def test_wrong_length_for_version(self):
        good = bytes.fromhex(self.GOOD)
        self.assertIsNone(lora_packet.decode(good[:-1]))       # truncated
        self.assertIsNone(lora_packet.decode(good + b"\x00"))  # trailing byte

    def test_magic_and_length_must_agree(self):
        """A newer magic on an older frame's length is a corrupted older frame,
        not a short newer one."""
        for older, magic in (("v2_full", lora_packet.MAGIC_V3),
                             ("v3_full", lora_packet.MAGIC_V4),
                             ("v4_full", lora_packet.MAGIC_V5)):
            buf = bytearray(bytes.fromhex(VECTORS[older][0]))
            buf[0] = magic
            with self.subTest(frame=older):
                self.assertIsNone(lora_packet.decode(bytes(buf)))

    def test_v5_truncated_to_v4_length_is_rejected(self):
        v5 = bytes.fromhex(self.GOOD)
        self.assertIsNone(lora_packet.decode(v5[:lora_packet.PKT_LEN_V4]))


class TestCrc(unittest.TestCase):
    def test_known_property(self):
        """CRC-8/SMBUS of a buffer followed by its own CRC is 0 — the cheap
        self-check the firmware relies on."""
        for hexstr, _ in VECTORS.values():
            buf = bytes.fromhex(hexstr)
            with self.subTest(frame=hexstr[:2]):
                self.assertEqual(lora_packet.crc8(buf), 0)


if __name__ == "__main__":
    unittest.main()
