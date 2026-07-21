#!/usr/bin/env python3
"""
DS3231 hardware RTC over I2C — shared read/write layer.

Used by ds3231_sync.py (chip -> system clock, at boot) and ds3231_writeback.py
(system clock -> chip, once NTP has settled). Kept dependency-light: smbus2 if
present, else the older smbus, matching what is already on the Jetson.

TIME CONVENTION
---------------
This module reads and writes the chip in **UTC** (RTC_STORES_UTC below). The
chip itself stores naked calendar fields with no zone information, so the
convention has to be agreed on out-of-band -- that is exactly what /etc/adjtime
does for `hwclock`, and what the original ds3231_sync.py left unstated.

UTC is chosen because it has no DST discontinuity. Asia/Bangkok has no DST
today, but storing local time bakes in an assumption that breaks silently if
the box is ever moved or the zone is ever changed.

If your chip currently holds LOCAL time, see docs/host-setup.md for the
one-time migration (run the writeback once while the system clock is correct).
"""
import time
from datetime import datetime

try:
    import smbus2 as smbus
except ImportError:  # older images ship python3-smbus only
    import smbus

BUS_NUM = 1          # Jetson Nano 40-pin header pins 3 (SDA) / 5 (SCL)
DS3231_ADDR = 0x68

REG_TIME = 0x00      # 7 bytes: sec, min, hour, dow, day, month(+century), year
REG_CONTROL = 0x0E
REG_STATUS = 0x0F    # bit 7 = OSF (oscillator stopped flag)

OSF_BIT = 0x80
HOUR_12_BIT = 0x40   # set = 12-hour mode
HOUR_PM_BIT = 0x20   # in 12-hour mode only
CENTURY_BIT = 0x80   # in the month register

RTC_STORES_UTC = True

# A read below this year means the chip lost power and reset to 2000-01-01, or
# the I2C read was garbled. Either way the value must not reach the system clock.
MIN_PLAUSIBLE_YEAR = 2025


class RtcError(Exception):
    """Raised when the chip is unreachable or its contents cannot be trusted."""


def _bcd_to_dec(bcd):
    return (bcd >> 4) * 10 + (bcd & 0x0F)


def _dec_to_bcd(dec):
    return ((dec // 10) << 4) | (dec % 10)


def _open_bus(retries, delay_s):
    """
    Open the I2C bus, retrying briefly.

    At sysinit.target the i2c-dev module may not be loaded yet, which surfaces
    as FileNotFoundError on /dev/i2c-1. One retry loop turns a permanently
    failed boot-time unit into a slightly slower successful one.
    """
    last = None
    for attempt in range(retries):
        try:
            return smbus.SMBus(BUS_NUM)
        except (FileNotFoundError, PermissionError, OSError) as exc:
            last = exc
            if attempt < retries - 1:
                time.sleep(delay_s)
    raise RtcError("cannot open I2C bus {}: {}".format(BUS_NUM, last))


def oscillator_stopped(bus):
    """
    True if the DS3231's OSF flag is set.

    OSF means the oscillator halted at some point -- flat backup cell, or the
    chip has never been set since it was soldered. The time registers are then
    meaningless (typically 2000-01-01 00:00:00) and must NOT be believed.
    """
    return bool(bus.read_byte_data(DS3231_ADDR, REG_STATUS) & OSF_BIT)


def clear_oscillator_flag(bus):
    """Clear OSF. Only valid immediately after writing a known-good time."""
    status = bus.read_byte_data(DS3231_ADDR, REG_STATUS)
    bus.write_byte_data(DS3231_ADDR, REG_STATUS, status & ~OSF_BIT)


def read_time(bus):
    """
    Read the chip's calendar as a naive datetime (in RTC_STORES_UTC's zone).

    Handles 12-hour mode correctly, including the 12 AM case that decodes to
    hour 0 -- the original script mapped it to 12, putting every midnight read
    twelve hours out.
    """
    data = bus.read_i2c_block_data(DS3231_ADDR, REG_TIME, 7)

    second = _bcd_to_dec(data[0] & 0x7F)
    minute = _bcd_to_dec(data[1] & 0x7F)

    hour_raw = data[2]
    if hour_raw & HOUR_12_BIT:
        hour = _bcd_to_dec(hour_raw & 0x1F)
        pm = bool(hour_raw & HOUR_PM_BIT)
        if hour == 12:
            hour = 12 if pm else 0        # 12 PM -> 12, 12 AM -> 0
        elif pm:
            hour += 12
    else:
        hour = _bcd_to_dec(hour_raw & 0x3F)

    day = _bcd_to_dec(data[4] & 0x3F)
    month = _bcd_to_dec(data[5] & 0x1F)
    year = _bcd_to_dec(data[6]) + (2100 if data[5] & CENTURY_BIT else 2000)

    try:
        return datetime(year, month, day, hour, minute, second)
    except ValueError as exc:
        raise RtcError("RTC returned an invalid date "
                       "({}-{}-{} {}:{}:{}): {}".format(
                           year, month, day, hour, minute, second, exc))


def write_time(bus, when):
    """
    Write `when` (naive, in RTC_STORES_UTC's zone) to the chip.

    Forces 24-hour mode so subsequent reads never depend on the AM/PM bit, then
    clears OSF to mark the contents trustworthy again.
    """
    bus.write_i2c_block_data(DS3231_ADDR, REG_TIME, [
        _dec_to_bcd(when.second),
        _dec_to_bcd(when.minute),
        _dec_to_bcd(when.hour),          # bit 6 clear => 24-hour mode
        _dec_to_bcd(when.isoweekday()),  # 1..7, cosmetic; nothing here reads it
        _dec_to_bcd(when.day),
        _dec_to_bcd(when.month),         # century bit clear => 20xx
        _dec_to_bcd(when.year % 100),
    ])
    clear_oscillator_flag(bus)


def read_checked(retries=5, delay_s=1.0):
    """
    Open the bus, refuse an untrustworthy chip, and return (datetime, bus).

    Raises RtcError if the oscillator stopped or the date is implausible --
    a missing time is recoverable, a confidently wrong one is not.
    """
    bus = _open_bus(retries, delay_s)
    try:
        if oscillator_stopped(bus):
            raise RtcError(
                "OSF set: the DS3231 oscillator stopped (backup cell flat, or "
                "never set). Its time is garbage; refusing to set the system "
                "clock. Replace the CR2032, then run ds3231_writeback.py.")
        now = read_time(bus)
        if now.year < MIN_PLAUSIBLE_YEAR:
            raise RtcError(
                "RTC reports {}, before the {} sanity floor -- treating as "
                "lost time.".format(now.isoformat(), MIN_PLAUSIBLE_YEAR))
        return now, bus
    except Exception:
        bus.close()
        raise
