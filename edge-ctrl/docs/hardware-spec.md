# Hardware & Pin Specification: NVIDIA Jetson Nano & Raspberry Pi 3 Model B

## 1. Executive Summary

This document specifies the hardware pinout, electrical connections, Linux `gpiochip` mappings, and environment variable configuration for deploying the **Smart Farm Control System** (`edge-ctrl`) on two target single-board computers:
1. **NVIDIA Jetson Nano** (4GB Developer Kit, ARM64, Ubuntu 18.04 LTS / L4T)
2. **Raspberry Pi 3 Model B** (BCM2837 ARMv8, 1GB RAM, Raspberry Pi OS 32/64-bit)

Both platforms utilize a standard 40-pin expansion header but differ in Linux kernel GPIO driver naming and line offset indexing. All hardware mappings are decoupled from binary code and configured via environment files (`.env`).

---

## 2. Hardware Component Overview

| Component | Protocol / Signal Type | Electrical Spec | Required External Parts |
| :--- | :--- | :--- | :--- |
| **DS3231 Real-Time Clock** | I²C Bus (`/dev/i2c-1`) | 3.3V / 5V VCC, 3.3V Logic | CR2032 Coin Cell Battery |
| **DHT22 (AM2302) Sensor** | Single-Wire Digital Data | 3.3V / 5V VCC, 3.3V Logic | 4.7kΩ – 10kΩ Pull-Up Resistor between VCC and DATA |
| **Relay Switch (External Fan)** | GPIO Digital Output | 5V VCC Power, 3.3V Control Signal | Opto-isolated Relay Module (Active-High or Active-Low) |

---

## 3. Side-by-Side Pin Mapping Matrix

| Signal Function | Physical Pin (40-Pin J8) | NVIDIA Jetson Nano | Raspberry Pi 3 Model B | Environment Variable |
| :--- | :--- | :--- | :--- | :--- |
| **DS3231 VCC** | Pin 1 (or Pin 2) | 3.3V (or 5V) Power | 3.3V (or 5V) Power | N/A |
| **DS3231 GND** | Pin 6 | Ground | Ground | N/A |
| **DS3231 I2C SDA** | Pin 3 | `I2C1_SDA` (`/dev/i2c-1`) | `BCM 2` (`/dev/i2c-1`) | `DS3231_I2C_BUS=1`, `DS3231_SDA_PIN=3` |
| **DS3231 I2C SCL** | Pin 5 | `I2C1_SCL` (`/dev/i2c-1`) | `BCM 3` (`/dev/i2c-1`) | `DS3231_I2C_BUS=1`, `DS3231_SCL_PIN=5` |
| **DHT22 VCC** | Pin 1 (or Pin 2) | 3.3V (or 5V) Power | 3.3V (or 5V) Power | N/A |
| **DHT22 GND** | Pin 9 | Ground | Ground | N/A |
| **DHT22 Data** | **Pin 7** | `TEGRA_GPIO(CC, 4)` | `BCM 4` (GPIO_GCLK) | `DHT22_GPIO_CHIP=gpiochip0`<br>`DHT22_GPIO_LINE=149` (Jetson)<br>`DHT22_GPIO_LINE=4` (RPi) |
| **Relay VCC** | Pin 2 (or Pin 4) | 5V Power | 5V Power | N/A |
| **Relay GND** | Pin 14 | Ground | Ground | N/A |
| **Relay Signal (IN)**| **Pin 11** | `TEGRA_GPIO(Z, 0)` | `BCM 17` (GPIO_GEN0) | `RELAY_GPIO_CHIP=gpiochip0`<br>`RELAY_GPIO_LINE=200` (Jetson)<br>`RELAY_GPIO_LINE=17` (RPi) |

---

## 4. Platform Specifications & Linux Drivers

### 4.1 NVIDIA Jetson Nano
- **GPIO Chip Driver:** `tegra-gpio` registered under `gpiochip0` (or `gpiochip1` depending on kernel tree).
- **Line Indexing:** Sysfs/Tegra calculation offset.
  - Pin 7 (`TEGRA_GPIO(CC, 4)`) $\rightarrow$ Line Offset `149`
  - Pin 11 (`TEGRA_GPIO(Z, 0)`) $\rightarrow$ Line Offset `200`
- **I²C Device File:** `/dev/i2c-1` (Bus 1) at address `0x68`.

### 4.2 Raspberry Pi 3 Model B
- **GPIO Chip Driver:** `pinctrl-bcm2835` registered as `gpiochip0` (54 lines total).
- **Standard GPIOs (Relay, DHT22):** Enabled by default in the Linux kernel — no `raspi-config` steps required for digital GPIO pins.
- **I²C Device File (DS3231 RTC):** `/dev/i2c-1` (Bus 1) at address `0x68`. Enable via terminal:
  ```bash
  sudo raspi-config nonint do_i2c 0
  ```
  or interactively: `sudo raspi-config` $\rightarrow$ **Interface Options** $\rightarrow$ **I2C** $\rightarrow$ **Enable**.
- **User Permissions:** Add your user to the `gpio` and `i2c` groups: `sudo usermod -aG gpio,i2c $USER`.

---

## 5. Environment Template Configurations

### 5.1 Jetson Nano Template (`.env.jetson_nano.template`)
```env
TARGET_DEVICE=jetson_nano

# DS3231 RTC
DS3231_I2C_BUS=1
DS3231_I2C_ADDR=0x68
DS3231_SDA_PIN=3
DS3231_SCL_PIN=5

# DHT22 Sensor
DHT22_GPIO_CHIP=gpiochip0
DHT22_GPIO_LINE=149
DHT22_PIN=149

# Relay Switch
RELAY_GPIO_CHIP=gpiochip0
RELAY_GPIO_LINE=200
RELAY_PIN=200
RELAY_ACTIVE_HIGH=true
```

### 5.2 Raspberry Pi 3B Template (`.env.raspberry_pi_3b.template`)
```env
TARGET_DEVICE=raspberry_pi_3b

# DS3231 RTC
DS3231_I2C_BUS=1
DS3231_I2C_ADDR=0x68
DS3231_SDA_PIN=3
DS3231_SCL_PIN=5

# DHT22 Sensor
DHT22_GPIO_CHIP=gpiochip0
DHT22_GPIO_LINE=4
DHT22_PIN=4

# Relay Switch
RELAY_GPIO_CHIP=gpiochip0
RELAY_GPIO_LINE=17
RELAY_PIN=17
RELAY_ACTIVE_HIGH=true
```

---

## 6. Installation & Verification Workflow

### 6.1 Deploying Config on Host Target
- **On Jetson Nano:**
  ```bash
  cd edge-ctrl/python
  sudo ./install.sh jetson
  ```
- **On Raspberry Pi 3B:**
  ```bash
  cd edge-ctrl/python
  sudo ./install.sh rpi
  ```

### 6.2 Testing & Diagnostics
1. **Verify I²C Bus Detection:**
   ```bash
   sudo i2cdetect -y -r 1
   ```
   *Expected output: Address `0x68` appears on bus 1.*

2. **Test DHT22 Pin Configuration:**
   ```bash
   python3 dht22.py
   ```

3. **Test Relay Switch Actuation:**
   ```bash
   python3 relay.py on
   python3 relay.py off
   ```

4. **Verify Boot Time Synchronization:**
   ```bash
   systemctl status ds3231-sync.service
   ```

---

## 7. Lessons Learned & Technical Notes

### 7.1 `gpiod` v1 vs `gpiod` v2 Library Compatibility
- **Python Scripts:** `python3-gpiod` v1 uses `chip.get_line()` and `chip.num_lines` as an integer property. `gpiod` 2.x (Debian 12 Bookworm) uses `gpiod.request_lines()` and `gpiod.LineSettings`. Python scripts dynamically inspect APIs at runtime with fallbacks to `gpioset` CLI (`gpioset /dev/gpiochip0 17=1`) and sysfs.
- **C++ Daemon:** CMake inspects `libgpiod` version via `pkg-config` and defines `HAVE_GPIOD_V2=1` (for `libgpiod >= 2.0.0`) or `HAVE_GPIOD_V1=1` (for `1.6.3`). C++ source files (`fan.cpp`, `dht22.cpp`) implement `#if HAVE_GPIOD_V2` using `gpiod_line_request` and `gpiod_line_settings`.

### 7.2 Character Device Path Resolution & Line Offset Bounds
- **Device Paths:** Passing bare string `"gpiochip0"` to `gpiod.Chip` on 64-bit kernels may fail with `[Errno 2] No such file or directory` if `/dev/gpiochip0` is missing or mapped to a different chip index (e.g. `/dev/gpiochip4`). Host scripts and C++ daemon resolve paths against `/dev/gpiochip*` nodes.
- **Line Offset Bounds:** Jetson Tegra line `200` causes `libgpiod` to return `EINVAL` (*Invalid argument*) on Raspberry Pi's 54-line `pinctrl-bcm2835` controller (0..53). Raspberry Pi configuration MUST specify `line_offset: 17` for Pin 11 (BCM 17) and `line_offset: 4` for Pin 7 (BCM 4).

### 7.3 Target-Specific Configuration Templates & Automated Installer
- **Templates:** Dedicated configuration templates are provided for each hardware platform:
  - [`config.raspberry_pi_3b.json`](../config.raspberry_pi_3b.json) (`external_fan.line_offset`: 17, `dht22.line_offset`: 4, `zone_names`: `["cpu-thermal"]`).
  - [`config.jetson_nano.json`](../config.jetson_nano.json) (`external_fan.line_offset`: 200, `dht22.line_offset`: 149, `zone_names`: `["CPU-therm", "GPU-therm"]`).
- **Installer Script:** Running `sudo ./install.sh rpi` or `sudo ./install.sh jetson` automatically deploys both `.env` and `config.json` templates to `/etc/edge-ctrl/`.

### 7.4 CMake `nlohmann_json` Multi-Stage Fallback
- `CMakeLists.txt` uses a 4-tier fallback for `nlohmann_json`: (1) vendored header `third_party/nlohmann/json.hpp`, (2) CMake package `find_package(nlohmann_json QUIET)`, (3) system include `/usr/include/nlohmann/json.hpp`, or (4) automatic remote header download directly into the build tree during `cmake ..`.

### 7.5 `raspi-config` Requirements
- **Standard GPIOs:** Enabled by default in kernel — no `raspi-config` steps required for digital GPIO pins.
- **I²C RTC (DS3231):** Must be enabled via `sudo raspi-config nonint do_i2c 0`.
- **User Groups:** Run `sudo usermod -aG gpio,i2c $USER` to grant non-root script execution rights.

