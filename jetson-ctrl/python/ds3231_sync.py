#!/usr/bin/env python3
"""
Set the Jetson's system clock from the DS3231 hardware RTC, at boot.

Runs before the network exists, so this is the only thing standing between a
headless reboot and a system clock of 1970. Replaces the original
/usr/local/bin/ds3231_sync.py; see docs/host-setup.md for what changed and why.

Exits non-zero WITHOUT touching the clock if the chip's time cannot be trusted.
A missing time is a visible failure that NTP will fix later; a wrong one is
silent and poisons every timestamp, TLS handshake and irrigation schedule
downstream.
"""
import subprocess
import sys

import ds3231


def set_system_clock(when):
    """Apply `when` via date(1). -u when the chip holds UTC."""
    stamp = when.strftime("%Y-%m-%d %H:%M:%S")
    cmd = ["date", "-u", "-s", stamp] if ds3231.RTC_STORES_UTC else \
          ["date", "-s", stamp]
    result = subprocess.run(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, universal_newlines=True)
    if result.returncode != 0:
        raise ds3231.RtcError(
            "date(1) refused to set the clock: {}".format(result.stderr.strip()))
    return stamp


def main():
    try:
        now, bus = ds3231.read_checked()
    except ds3231.RtcError as exc:
        print("ds3231-sync: {}".format(exc), file=sys.stderr)
        return 1
    bus.close()

    try:
        stamp = set_system_clock(now)
    except ds3231.RtcError as exc:
        print("ds3231-sync: {}".format(exc), file=sys.stderr)
        return 1

    zone = "UTC" if ds3231.RTC_STORES_UTC else "local"
    print("ds3231-sync: system clock set from DS3231: {} ({})".format(stamp, zone))
    return 0


if __name__ == "__main__":
    sys.exit(main())
