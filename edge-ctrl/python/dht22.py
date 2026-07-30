#!/usr/bin/env python3
"""
DHT22 Temperature and Humidity Sensor interface for Edge Devices.
Reads pin mapping from environment variables (.env / env_config).
Auto-detects available /dev/gpiochip* devices on Raspberry Pi and Jetson.
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


def find_working_chip(requested_chip, line_offset):
    """
    Finds an accessible gpiod.Chip instance that contains line_offset.
    Checks requested_chip, /dev/<chip>, and all /dev/gpiochip* nodes.
    """
    try:
        import gpiod
    except ImportError:
        return None, None

    candidates = [
        requested_chip,
        "/dev/" + os.path.basename(requested_chip) if not requested_chip.startswith("/") else requested_chip,
    ]
    for c in sorted(glob.glob("/dev/gpiochip*")):
        if c not in candidates:
            candidates.append(c)

    last_error = None
    for cand in candidates:
        try:
            chip = gpiod.Chip(cand)
            if line_offset < chip.num_lines():
                return chip, cand
            else:
                chip.close()
        except Exception as exc:
            last_error = exc
            continue

    return None, None


def get_pin_info():
    """Returns configured pin mapping details for DHT22."""
    chip_obj, resolved_chip = find_working_chip(GPIO_CHIP, GPIO_LINE)
    if chip_obj:
        chip_obj.close()

    return {
        "sensor": "DHT22",
        "gpio_chip": GPIO_CHIP,
        "resolved_chip": resolved_chip or GPIO_CHIP,
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
