#!/usr/bin/env python3
"""
Write the NTP-corrected system clock back into the DS3231.

The half that was missing from the original setup: without it the chip is
write-once -- set by hand at install time and free-running ever since, with no
path for the network to ever correct it. A DS3231 is a good part (+/-2 ppm) but
that is still ~1 min/year, accumulating forever.

Guarded on NTP actually having synchronised. Writing an unsynchronised system
clock back would, at best, rewrite the chip's own drifted value onto itself and,
at worst, persist a bogus time into the only offline time source on the box.

Deliberately NOT `hwclock -w`: this DS3231 has no kernel driver bound (the sync
script talks raw I2C), so there is no /dev/rtcN for hwclock to write to.
"""
import subprocess
import sys
from datetime import datetime

import ds3231


def ntp_synchronised():
    """True if systemd believes the clock has been set from the network."""
    result = subprocess.run(
        ["timedatectl", "show", "-p", "NTPSynchronized", "--value"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        universal_newlines=True)
    if result.returncode == 0:
        return result.stdout.strip() == "yes"

    # Older systemd (18.04's is new enough, but be forgiving) has no `show -p`.
    fallback = subprocess.run(["timedatectl", "status"], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, universal_newlines=True)
    return "synchronized: yes" in fallback.stdout.lower()


def main():
    force = "--force" in sys.argv

    if not force and not ntp_synchronised():
        print("ds3231-writeback: NTP not synchronised; leaving the RTC alone.",
              file=sys.stderr)
        return 1

    now = datetime.utcnow() if ds3231.RTC_STORES_UTC else datetime.now()

    try:
        bus = ds3231._open_bus(retries=5, delay_s=1.0)
    except ds3231.RtcError as exc:
        print("ds3231-writeback: {}".format(exc), file=sys.stderr)
        return 1

    try:
        ds3231.write_time(bus, now)
    except OSError as exc:
        print("ds3231-writeback: I2C write failed: {}".format(exc),
              file=sys.stderr)
        return 1
    finally:
        bus.close()

    zone = "UTC" if ds3231.RTC_STORES_UTC else "local"
    print("ds3231-writeback: DS3231 set to {} ({}){}".format(
        now.strftime("%Y-%m-%d %H:%M:%S"), zone,
        " [forced]" if force else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
