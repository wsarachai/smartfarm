#!/usr/bin/env python3
"""
DHT22 Temperature and Humidity Sensor interface for Edge Devices.
Reads pin mapping from environment variables (.env / env_config).
Supports gpiod v1 and v2 APIs across Jetson and Raspberry Pi.
"""
import glob
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


def get_candidate_chip_paths(requested_chip):
    """Returns a list of candidate gpiochip device paths to try."""
    candidates = []
    req_str = str(requested_chip)

    if req_str.startswith("/"):
        candidates.append(req_str)
    else:
        candidates.append("/dev/" + os.path.basename(req_str))
        candidates.append(req_str)

    for c in sorted(glob.glob("/dev/gpiochip*")):
        if c not in candidates:
            candidates.append(c)

    return candidates


def resolve_dht22_chip(requested_chip):
    """Resolves accessible gpiochip path for DHT22."""
    candidates = get_candidate_chip_paths(requested_chip)
    for chip_path in candidates:
        if os.path.exists(chip_path) or os.path.exists("/dev/" + os.path.basename(chip_path)):
            return chip_path
    return requested_chip


def get_pin_info():
    """Returns configured pin mapping details for DHT22."""
    resolved_chip = resolve_dht22_chip(GPIO_CHIP)
    return {
        "sensor": "DHT22",
        "gpio_chip": GPIO_CHIP,
        "resolved_chip": resolved_chip,
        "gpio_line": GPIO_LINE,
        "pin": PIN,
        "available_chips": sorted(glob.glob("/dev/gpiochip*"))
    }


def main():
    info = get_pin_info()
    print("DHT22 Sensor Pin Mapping (from environment):")
    print("  Configured Chip: {}".format(info["gpio_chip"]))
    print("  Resolved Chip  : {}".format(info["resolved_chip"]))
    print("  GPIO Line      : {}".format(info["gpio_line"]))
    print("  Pin Number     : {}".format(info["pin"]))
    print("  Available Chips: {}".format(", ".join(info["available_chips"]) if info["available_chips"] else "none found"))


if __name__ == "__main__":
    main()
