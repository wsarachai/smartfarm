# Jetson Nano host setup

Everything the `jetson-ctrl` daemon assumes about the machine underneath it:
the I²C real-time clock, the 40-pin header allocation, and the boot-time units
that hold it together.

> **Why this file exists.** All of this was originally configured by hand,
> directly on the Jetson, and existed nowhere else. After a reflash it would
> have been unrecoverable, and in July 2026 it took a filesystem-wide search to
> reconstruct what had been done. Host state is part of the system; it belongs
> in version control.

## Hardware inventory

| Device | Interface | Header pins | Notes |
|---|---|---|---|
| DS3231 RTC | I²C bus 1, addr `0x68` | 1 (3.3V), 3 (SDA), 5 (SCL), 6 (GND) | CR2032 backup cell |
| DHT22 | bit-banged GPIO | see below | enclosure air temp/humidity |
| External fan relay | GPIO | see below | second cooling stage |

Confirm the RTC is on the bus:

```bash
sudo i2cdetect -y -r 1     # 0x68 should appear
```

`UU` instead of `68` means a kernel driver claimed it — see
[No kernel driver, by design](#no-kernel-driver-by-design).

## GPIO allocation

Jetson Nano `gpiochip0` line offsets equal the legacy sysfs numbers, so
`gpioinfo` offsets map directly onto the values in `config.example.json`.

| Function | Header pin | `gpiochip0` line | Label | Config key |
|---|---|---|---|---|
| DHT22 data | 29 | 149 | `GPIO01` | `dht22.line_offset` |
| External fan | 31 | 200 | `GPIO11` | `external_fan.line_offset` |

Both verified `unused` on the target with `gpioinfo` (2026-07-21).

**Pin 29 rather than pin 7** for the DHT22: pin 7 was believed to be occupied by
the RTC wiring. Pin 29 is a plain GPIO with no default alt-function and sits
beside GND on pin 30, which keeps the bit-banged data run short.

**Pin 31 rather than pin 15** for the fan. Line 194 (pin 15) is **claimed by the
SD card-detect driver** — `gpioinfo` shows it as `"cd" [used]`. `libgpiod` would
refuse the request at daemon startup, and driving it would interfere with the
boot device. The original `194` in `config.example.json` was a placeholder that
had never been checked against real hardware. Pin 31 is plain GPIO, and leaves
the PWM-capable line 38 (pin 33) free should the relay ever be replaced with a
variable-speed fan.

Always confirm before wiring — placeholders lie, and `libgpiod` fails the whole
daemon at startup if a line is held:

```bash
sudo apt install gpiod                                 # tools, separate from libgpiod-dev
sudo gpioinfo gpiochip0 | grep -E 'line +(149|200):'   # want "unused", no [used]
```

Known-free alternates on this board, all verified unused: lines 12, 14, 15, 38,
50, 51, 76, 77, 78, 79, 232.

> **libgpiod 1.1 syntax.** JetPack 4.x ships libgpiod 1.1, which takes
> positional arguments — `gpioinfo gpiochip0`, `gpioget gpiochip0 149`. Examples
> online are usually written for 1.6+ or 2.x and use `--chip` flags that do not
> exist here.

### DHT22 wiring

| DHT22 | Jetson |
|---|---|
| VCC | pin 17 — **3.3V**, not 5V |
| DATA | pin 29, with a 10 kΩ pull-up to pin 17 |
| GND | pin 30 |

3.3V matters: the DATA line idles at VCC, so a 5V-powered sensor drives 5V into
a 3.3V-only Tegra pin. Many breakout boards already carry the pull-up — check
before adding a second.

Keep the data wire short and away from the fan's power leads. Motor noise
coupling into a bit-banged 1-wire line shows up as checksum failures, which the
daemon's retry logic reports as *stale* rather than as the wiring fault it is.

### Fan relay wiring

Never drive a relay coil from a header pin — Nano GPIOs are 3.3V at a few mA, a
typical 5V coil wants ~70 mA. Use an opto-isolated relay module or a
logic-level MOSFET, powered separately, sharing ground with the Nano.

**The as-built rig uses an active-low relay module**, so `external_fan.active_high`
is `false`: logical ON drives line 200 **low**. The symptom that identified it was
the fan running whenever the daemon was stopped — an unclaimed line reverts to
input and floats/pulls low, which an active-low `IN` reads as asserted. Note this
means the fan runs during any window where the daemon is not holding the line,
including the gap between power-on and `jetson-ctrl` starting.

Bench-test polarity before connecting the fan, and set `external_fan.active_high`
to match:

```bash
sudo gpioset gpiochip0 200=1     # fan should energise if active_high is true
```

Note `gpioset` holds the line only while it runs; on exit the line is released
and reverts. Use `gpioset -m wait gpiochip0 200=1` to hold it until you press
enter.

Mind the boot window. The line floats until the daemon claims it; `fan.cpp` is
fail-to-ON, so make sure your chosen polarity means *fan running* during that
gap, not *fan stopped*.

## Timekeeping

The Nano has no battery-backed clock of its own and often boots with no network,
so without the DS3231 it comes up in 1970. Two units, moving time in opposite
directions:

```
boot ──► ds3231-sync.service ──────► system clock      (chip → system, no network)
              (sysinit.target)

network up ──► ds3231-writeback.service ──► DS3231     (system → chip, NTP-corrected)
              (multi-user.target, + daily timer)
```

`ds3231-sync` alone is not enough. Without the writeback the chip is
effectively write-once — set by hand at install, free-running forever after.
At ±2 ppm that is ~1 minute per year, and every boot overwrites a good system
clock with the drifted value.

### Install

```bash
cd jetson-ctrl/python
sudo ./install.sh
```

That places `ds3231.py`, `ds3231_sync.py` and `ds3231_writeback.py` in
`/usr/local/bin`, installs all three units, ensures `i2c-dev` loads at boot, and
enables `ds3231-sync.service` plus `ds3231-writeback.timer`.

### The UTC convention

The DS3231 stores bare calendar fields with **no timezone information**. The
convention has to be agreed out-of-band — this is precisely what `/etc/adjtime`
records for `hwclock`.

These scripts use **UTC** (`RTC_STORES_UTC` in `ds3231.py`). Local time would
bake in an assumption that breaks silently if the box moves or its zone changes.

The original script used `date -s`, which interprets its argument in **local
time** — so a chip written under the old setup most likely holds Asia/Bangkok
local time. Migrate it once, while the system clock is known good:

```bash
timedatectl                         # confirm the system clock is right first
sudo /usr/bin/python3 /usr/local/bin/ds3231_writeback.py --force
```

Skip that and the first boot after installing will set the clock 7 hours off.

### Verify

```bash
sudo systemctl start ds3231-sync
systemctl status ds3231-sync
journalctl -u ds3231-sync -b
systemctl list-timers ds3231-writeback.timer
```

## What changed from the original setup

The original was `/usr/local/bin/ds3231_sync.py` plus a hand-written
`ds3231-sync.service`. Both worked, and both had defects worth recording:

**Oscillator-stopped flag was never checked.** The DS3231's OSF bit (status
register `0x0F`, bit 7) means "my oscillator halted, this time is garbage."
When the CR2032 dies the chip returns `2000-01-01 00:00:00`, and the original
script set the system clock to it. With `RemainAfterExit=yes` that sticks for
the whole boot. Downstream: TLS fails with "certificate not yet valid", `apt`
refuses to update, telemetry timestamps land 26 years in the past, and
`irrigationScheduler.js` computes weekdays against the year 2000 — so watering
fires on the wrong days, or never, with the moisture guard masking it as a
routine skip. `ds3231.read_checked()` now refuses, exits non-zero, and leaves
the clock alone. A missing time is a visible fault NTP will repair; a
confidently wrong one is silent.

**A plausibility floor.** Even with OSF clear, a garbled I²C read can decode to
nonsense. Anything before `MIN_PLAUSIBLE_YEAR` (2025) is rejected.

**The timezone convention was unstated.** Covered above.

**A 12-hour-mode decode bug.** The original mapped 12 AM to hour 12 instead of
0, putting every midnight-to-1am read twelve hours out. Only reachable if
something puts the chip in 12-hour mode — the default is 24 — but `write_time()`
now forces 24-hour mode explicitly, and `read_time()` handles both correctly.

**A boot race on `i2c-dev`.** The unit ran at `sysinit.target` ordered only
`After=local-fs.target`. If `i2c-dev` had not loaded, opening `/dev/i2c-1`
raised `FileNotFoundError`, the unit failed permanently for that boot, and the
system silently ran on a wrong clock. Now ordered
`After=systemd-modules-load.service`, with a retry loop in `_open_bus()` as a
second line of defence.

**No writeback existed.** Added, guarded on `NTPSynchronized`.

### No kernel driver, by design

There is no `rtc-ds1307` overlay bound to this chip, so there is no `/dev/rtc1`
and `hwclock` cannot see it — which is why the writeback talks raw I²C rather
than calling `hwclock -w`.

A device-tree overlay would be the more conventional approach, and would let
systemd restore the clock before userspace runs at all. It is a bigger change
(rebuilding a `.dtbo`, editing `/boot/extlinux/extlinux.conf`) and the
userspace path is already working, so it stays as-is. If you ever do add the
overlay, both scripts become redundant — remove them rather than letting two
mechanisms fight over one chip.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `i2cdetect` shows nothing at `0x68` | Wiring, or the module has no power. Check pins 3/5, and 3.3V on pin 1. |
| `OSF set` in the journal | Flat CR2032. Replace it, then run the writeback with `--force`. |
| Clock is 7 hours out after install | The chip still holds local time — run the UTC migration above. |
| `ds3231-sync` failed, `FileNotFoundError` | `i2c-dev` not loaded. `lsmod \| grep i2c_dev`; `install.sh` writes `/etc/modules-load.d/ds3231.conf`. |
| Writeback exits 1 every boot | Expected when offline. It only writes once NTP has synchronised. |
| Time correct at boot, drifts over weeks | Timer not enabled: `systemctl list-timers ds3231-writeback.timer`. |
