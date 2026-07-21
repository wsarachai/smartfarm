# jetson-ctrl/python — host provisioning

Python host tooling for the Jetson Nano the `jetson-ctrl` daemon runs on.
Separate from the C++ daemon on purpose: this is **one-shot host setup** that
runs at boot and exits, not part of the control loop.

| File | Role |
|---|---|
| `ds3231.py` | DS3231 I²C layer — BCD codec, OSF check, read/write |
| `ds3231_sync.py` | boot: DS3231 → system clock (before the network exists) |
| `ds3231_writeback.py` | online: NTP-corrected system clock → DS3231 |
| `systemd/` | the three units (`sync.service`, `writeback.service`, `writeback.timer`) |
| `install.sh` | copies scripts + units into place, enables them |

```bash
sudo ./install.sh
```

Full wiring, the UTC convention, the one-time migration, and the defect record
for the original hand-written version: **[../docs/host-setup.md](../docs/host-setup.md)**.

## Notes

- **No third-party dependencies.** `smbus2` if present, else `smbus`. Targets
  the Python 3.6 that ships with Ubuntu 18.04 / L4T — no f-string `=`, no
  `subprocess.run(capture_output=)`.
- **Refuses rather than guesses.** A stopped oscillator or an implausible date
  exits non-zero with the clock untouched. NTP can repair a missing time; a
  silently wrong one poisons TLS, `apt`, telemetry timestamps and the
  irrigation schedule.
- **Not yet verified on hardware** — written from the running configuration on
  `watcharin-desktop` but not re-run there. Test with
  `sudo systemctl start ds3231-sync` and read the journal before rebooting.
