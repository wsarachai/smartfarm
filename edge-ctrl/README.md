# edge-ctrl

Native C++17 host daemon & Python host provisioning tooling for SmartFarm single-board computers (**NVIDIA Jetson Nano** & **Raspberry Pi 3 Model B**).

Manages the enclosure's **external cooling fan**, ingests thermal state & DHT22 enclosure telemetry, synchronizes the DS3231 hardware RTC clock at boot, reports health to the Node web-server, and self-heals under systemd.

> Not ESP firmware and not the Node server — this is a native Linux system daemon that runs on the edge device hosting the web control center. See **[DESIGN.md](DESIGN.md)** for full architecture rationale.

---

## Directory Layout

```
edge-ctrl/
├── src/ tests/ third_party/   # Native C++17 daemon & unit tests
├── CMakeLists.txt             # Multi-target build script
├── systemd/edge-ctrl.service  # Systemd service unit for C++ daemon
├── config.example.json        # Standard configuration template
├── config.raspberry_pi_3b.json # Pre-configured template for Raspberry Pi 3B
├── config.jetson_nano.json    # Pre-configured template for NVIDIA Jetson Nano
├── python/                    # Host provisioning tooling & diagnostic scripts
│   ├── env_config.py          # Zero-dependency .env environment parser
│   ├── .env.jetson_nano.template # NVIDIA Jetson Nano pin mapping template
│   ├── .env.raspberry_pi_3b.template # Raspberry Pi 3B pin mapping template
│   ├── ds3231.py / ds3231_sync.py # DS3231 I²C RTC time synchronization
│   ├── dht22.py               # DHT22 sensor diagnostic & pin loader
│   ├── relay.py               # External fan relay CLI test & state setter
│   ├── install.sh             # Multi-target host installer script
│   └── systemd/               # RTC sync systemd service & timer units
├── docs/hardware-spec.md      # Pinout matrix, electrical specs & lessons learned
├── docs/host-setup.md         # Wiring guide & boot unit documentation
└── docs/config-reference.md   # Hot-reloaded JSON configuration manual
```

---

## Hardware Specification & Pin Matrix

Complete electrical wiring details and kernel driver specifications are documented in **[docs/hardware-spec.md](docs/hardware-spec.md)**.

| Signal Function | Physical Pin (40-Pin Header) | NVIDIA Jetson Nano | Raspberry Pi 3 Model B | Environment Configuration |
| :--- | :--- | :--- | :--- | :--- |
| **DS3231 SDA** | Pin 3 | Bus `1` (`I2C1_SDA`) | Bus `1` (`BCM 2`) | `DS3231_I2C_BUS=1`, `DS3231_SDA_PIN=3` |
| **DS3231 SCL** | Pin 5 | Bus `1` (`I2C1_SCL`) | Bus `1` (`BCM 3`) | `DS3231_I2C_BUS=1`, `DS3231_SCL_PIN=5` |
| **DHT22 Data** | Pin 7 | `gpiochip0` Line `149` | `gpiochip0` Line `4` (`BCM 4`) | `DHT22_GPIO_CHIP=gpiochip0`, `DHT22_GPIO_LINE=4` (RPi) / `149` (Jetson) |
| **Relay Signal (IN)** | Pin 11 | `gpiochip0` Line `200` | `gpiochip0` Line `17` (`BCM 17`) | `RELAY_GPIO_CHIP=gpiochip0`, `RELAY_GPIO_LINE=17` (RPi) / `200` (Jetson) |

---

## 1. Host Provisioning (Python Tooling)

The [`python/`](python/) tree handles one-shot boot provisioning for environment variables (`.env`), target hardware configuration (`/etc/edge-ctrl/config.json`), I²C RTC timekeeping, and pin diagnostics.

### Install on NVIDIA Jetson Nano
```bash
cd python
sudo ./install.sh jetson
```

### Install on Raspberry Pi 3 Model B
```bash
cd python
sudo ./install.sh rpi
```
*Note: For I²C RTC operation on Raspberry Pi, ensure I²C is enabled via `sudo raspi-config nonint do_i2c 0`.*

### Hardware Diagnostic Verification
Test relay actuation & GPIO chip detection directly:
```bash
sudo python3 relay.py on
sudo python3 relay.py off
sudo python3 relay.py --diag
python3 dht22.py
```

---

## 2. Building & Deploying the C++ Daemon

### Build Prerequisites
```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev libgpiod-dev
```

### Compile
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest
```

### Install Service Daemon
```bash
sudo make -C build install
sudo mkdir -p /etc/edge-ctrl

# Copy target-specific configuration template if not deployed by install.sh:
# For Raspberry Pi 3B:
sudo cp config.raspberry_pi_3b.json /etc/edge-ctrl/config.json
# For Jetson Nano:
# sudo cp config.jetson_nano.json /etc/edge-ctrl/config.json

sudo systemctl daemon-reload

# Foreground bench test before handing to systemd
sudo /usr/local/sbin/edge-ctrl --config /etc/edge-ctrl/config.json

# Enable and start systemd service to run automatically at boot
sudo systemctl enable --now edge-ctrl

# Verify service status and follow live logs
systemctl status edge-ctrl
journalctl -u edge-ctrl -f
```

---

## 3. Configuration & Control

- **Live Config:** `/etc/edge-ctrl/config.json` (**Hot-reloaded** on save without restarting the daemon).
- **Config Manual:** See **[docs/config-reference.md](docs/config-reference.md)** for parameters, thresholds, and safety rules.
- **Remote Override:** Dashboard command (`POST /api/v1/control`, `device_id: edge_ctrl_rpi3b` or `edge_ctrl_jetson`):
  ```json
  { "external_fan_override": "force_on", "until": "2026-07-20T15:30:00Z" }
  ```
