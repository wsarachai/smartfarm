# jetson-ctrl/python — host provisioning

Python host tooling for the Jetson Nano the `jetson-ctrl` daemon runs on.
Separate from the C++ daemon on purpose: this is **one-shot host setup** that
runs at boot and exits, not part of the control loop.

| File | Role |
|---|---|
| `env_config.py` | Environment variable loader (`.env` file parser for Python 3.6+) |
| `.env` / `.env.example` | Hardware pin & bus mapping configuration for DS3231, DHT22, and Relay |
| `ds3231.py` | DS3231 I²C layer — BCD codec, OSF check, read/write (uses `DS3231_I2C_BUS` & `DS3231_I2C_ADDR`) |
| `ds3231_sync.py` | boot: DS3231 → system clock (before the network exists) |
| `ds3231_writeback.py` | online: NTP-corrected system clock → DS3231 |
| `dht22.py` | DHT22 sensor interface (uses `DHT22_GPIO_CHIP` & `DHT22_GPIO_LINE` / `DHT22_PIN`) |
| `relay.py` | Relay switch / external fan control interface (uses `RELAY_GPIO_CHIP` & `RELAY_GPIO_LINE` / `RELAY_PIN`) |
| `systemd/` | the three units (`sync.service`, `writeback.service`, `writeback.timer`) |
| `install.sh` | copies scripts + units + `.env` config into place, enables them |

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
- **Installed and running** on the target (2026-07-21). `install.sh` completed,
  the chip was migrated to the UTC convention with
  `ds3231_writeback.py --force`, and the system clock comes up correct. **The
  offline case is still unproven** — no reboot with the network unplugged, which
  is the scenario the DS3231 exists for. Verify with
  `journalctl -u ds3231-sync -b`: the sync line must appear *before* any network
  or timesyncd line, or NTP is what fixed your clock, not the RTC.
