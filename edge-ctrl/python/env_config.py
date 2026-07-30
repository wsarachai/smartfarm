#!/usr/bin/env python3
"""
Environment configuration loader for jetson-ctrl host scripts.
Loads environment variables from .env files without third-party dependencies.
Targets Python 3.6 compatibility for Ubuntu 18.04 LTS on NVIDIA Jetson.
"""
import os
import sys


def load_env_file(filepath=None):
    """Load key-value pairs from a .env file into os.environ if not already set."""
    if filepath is None:
        search_paths = [
            os.getenv("EDGE_CTRL_ENV", os.getenv("JETSON_CTRL_ENV")),
            os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env"),
            "/etc/edge-ctrl/.env",
            "/etc/jetson-ctrl/.env",
            ".env",
            os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env.jetson_nano.template"),
            os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env.raspberry_pi_3b.template"),
        ]
        for path in search_paths:
            if path and os.path.isfile(path):
                filepath = path
                break

    if not filepath or not os.path.isfile(filepath):
        return False

    try:
        with open(filepath, "r") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip().strip("'\"")
                if key and key not in os.environ:
                    os.environ[key] = value
        return True
    except Exception as exc:
        print("Warning: failed to load env file {}: {}".format(filepath, exc), file=sys.stderr)
        return False


# Auto-load on module import
load_env_file()


def _get_int(key, default):
    val = os.getenv(key)
    if val is None or val == "":
        return default
    try:
        if val.startswith("0x") or val.startswith("0X"):
            return int(val, 16)
        return int(val)
    except ValueError:
        return default


def _get_str(key, default):
    return os.getenv(key, default)


# --- DS3231 Hardware RTC Pin & Bus Configuration ---
DS3231_I2C_BUS = _get_int("DS3231_I2C_BUS", _get_int("BUS_NUM", 1))
DS3231_I2C_ADDR = _get_int("DS3231_I2C_ADDR", _get_int("DS3231_ADDR", 0x68))
DS3231_SDA_PIN = _get_int("DS3231_SDA_PIN", 3)
DS3231_SCL_PIN = _get_int("DS3231_SCL_PIN", 5)

# --- DHT22 Sensor Pin Configuration ---
DHT22_GPIO_CHIP = _get_str("DHT22_GPIO_CHIP", "gpiochip0")
DHT22_GPIO_LINE = _get_int("DHT22_GPIO_LINE", _get_int("DHT22_PIN", 149))

# --- Relay Switch Pin Configuration ---
RELAY_GPIO_CHIP = _get_str("RELAY_GPIO_CHIP", "gpiochip0")
RELAY_GPIO_LINE = _get_int("RELAY_GPIO_LINE", _get_int("RELAY_PIN", 200))
RELAY_ACTIVE_HIGH = _get_str("RELAY_ACTIVE_HIGH", "true").lower() in ("true", "1", "yes")
