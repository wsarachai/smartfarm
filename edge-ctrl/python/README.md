# edge-ctrl/python — Multi-Device Host Provisioning

Python host tooling for single-board computers (**NVIDIA Jetson Nano** and **Raspberry Pi 3 Model B**) running the `edge-ctrl` daemon.

Separate from the C++ daemon on purpose: this is **one-shot host setup** that runs at boot and exits, not part of the control loop. All hardware pins and I²C bus configurations are decoupled into environment files (`.env`).

---

## File Structure & Roles

| File | Role |
| :--- | :--- |
| `env_config.py` | Environment loader (`.env` parser for Python 3.6+ without external dependencies) |
| `.env.jetson_nano.template` | Pin & bus mapping template for **NVIDIA Jetson Nano** |
| `.env.raspberry_pi_3b.template` | Pin & bus mapping template for **Raspberry Pi 3 Model B** |
| `ds3231.py` | DS3231 I²C layer — BCD codec, OSF check, read/write (uses `DS3231_I2C_BUS` & `DS3231_I2C_ADDR`) |
| `ds3231_sync.py` | Boot: DS3231 → system clock (before network connection exists) |
| `ds3231_writeback.py` | Online: NTP-corrected system clock → DS3231 |
| `dht22.py` | DHT22 sensor interface (uses `DHT22_GPIO_CHIP` & `DHT22_GPIO_LINE` / `DHT22_PIN`) |
| `relay.py` | Relay switch / fan control interface (uses `RELAY_GPIO_CHIP`, `RELAY_GPIO_LINE`, `RELAY_ACTIVE_HIGH`) |
| `systemd/` | Systemd service and timer units (`ds3231-sync.service`, `ds3231-writeback.service`, `ds3231-writeback.timer`) |
| `install.sh` | Deployment script — auto-detects hardware target or accepts `jetson` / `rpi` argument |

---

## Supported Hardware & Pinout Summary

For complete electrical wiring and Linux `gpiochip` driver details, see **[../docs/hardware-spec.md](../docs/hardware-spec.md)**.

| Signal Function | Physical Pin (40-Pin Header) | NVIDIA Jetson Nano Config | Raspberry Pi 3 Model B Config |
| :--- | :--- | :--- | :--- |
| **DS3231 I2C SDA** | Pin 3 | Bus `1` (`I2C1_SDA`) | Bus `1` (`BCM 2`) |
| **DS3231 I2C SCL** | Pin 5 | Bus `1` (`I2C1_SCL`) | Bus `1` (`BCM 3`) |
| **DHT22 Data** | Pin 7 | `gpiochip0` Line `149` | `gpiochip0` Line `4` (`BCM 4`) |
| **Relay Signal (IN)** | Pin 11 | `gpiochip0` Line `200` | `gpiochip0` Line `17` (`BCM 17`) |

---

## Installation & Deployment

### 1. Deploy on NVIDIA Jetson Nano
```bash
sudo ./install.sh jetson
```
*Installs scripts, systemd units, and copies `.env.jetson_nano.template` to `/etc/edge-ctrl/.env`.*

### 2. Deploy on Raspberry Pi 3 Model B
```bash
sudo ./install.sh rpi
```
*Installs scripts, systemd units, and copies `.env.raspberry_pi_3b.template` to `/etc/edge-ctrl/.env`.*

---

## Testing & Diagnostics

Verify hardware pin config and communication on the target device:

```bash
# 1. Verify I2C RTC detection (address 0x68 must appear)
sudo i2cdetect -y -r 1

# 2. Test DHT22 pin configuration
python3 dht22.py

# 3. Test Relay Switch actuation
python3 relay.py on
python3 relay.py off

# 4. Check RTC boot sync service
sudo systemctl start ds3231-sync
journalctl -u ds3231-sync -b
```

---

## Technical Notes

- **No Third-Party Dependencies:** Uses standard Python library + native `smbus`/`smbus2` and `libgpiod`. Targets Python 3.6+ on Ubuntu 18.04 / L4T and Raspberry Pi OS.
- **Refuses Rather Than Guesses:** A stopped oscillator (OSF set) or implausible date exits non-zero without poisoning system time.
- **Full Architecture Spec:** **[../docs/hardware-spec.md](../docs/hardware-spec.md)**.
