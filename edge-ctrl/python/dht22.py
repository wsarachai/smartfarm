#!/usr/bin/env python3
"""
DHT22 Temperature and Humidity Sensor interface for Jetson.
Reads pin mapping from environment variables (.env / env_config).
"""
import os
import sys

try:
    import env_config
    GPIO_CHIP = env_config.DHT22_GPIO_CHIP
    GPIO_LINE = env_config.DHT22_GPIO_LINE
    PIN = env_config.DHT22_GPIO_LINE
except ImportError:
    GPIO_CHIP = os.getenv("DHT22_GPIO_CHIP", "gpiochip0")
    _line_str = os.getenv("DHT22_GPIO_LINE", os.getenv("DHT22_PIN", "149"))
    GPIO_LINE = int(_line_str)
    PIN = GPIO_LINE


def get_pin_info():
    """Returns configured pin mapping details for DHT22."""
    return {
        "sensor": "DHT22",
        "gpio_chip": GPIO_CHIP,
        "gpio_line": GPIO_LINE,
        "pin": PIN
    }


def main():
    info = get_pin_info()
    print("DHT22 Sensor Pin Mapping (from environment):")
    print("  GPIO Chip : {}".format(info["gpio_chip"]))
    print("  GPIO Line : {}".format(info["gpio_line"]))
    print("  Pin Number: {}".format(info["pin"]))


if __name__ == "__main__":
    main()
