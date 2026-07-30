#!/usr/bin/env python3
"""
Relay Switch / External Fan Controller interface for Jetson.
Reads pin mapping from environment variables (.env / env_config).
"""
import os
import sys

try:
    import env_config
    GPIO_CHIP = env_config.RELAY_GPIO_CHIP
    GPIO_LINE = env_config.RELAY_GPIO_LINE
    PIN = env_config.RELAY_GPIO_LINE
    ACTIVE_HIGH = env_config.RELAY_ACTIVE_HIGH
except ImportError:
    GPIO_CHIP = os.getenv("RELAY_GPIO_CHIP", "gpiochip0")
    _line_str = os.getenv("RELAY_GPIO_LINE", os.getenv("RELAY_PIN", "200"))
    GPIO_LINE = int(_line_str)
    PIN = GPIO_LINE
    ACTIVE_HIGH = os.getenv("RELAY_ACTIVE_HIGH", "true").lower() in ("true", "1", "yes")


def get_pin_info():
    """Returns configured pin mapping details for Relay Switch."""
    return {
        "switch": "Relay",
        "gpio_chip": GPIO_CHIP,
        "gpio_line": GPIO_LINE,
        "pin": PIN,
        "active_high": ACTIVE_HIGH
    }


def set_relay_state(state_on):
    """
    Set Relay Switch state ON (True) or OFF (False).
    Uses libgpiod to set line value according to active_high configuration.
    """
    target_value = (1 if ACTIVE_HIGH else 0) if state_on else (0 if ACTIVE_HIGH else 1)
    try:
        import gpiod
        chip = gpiod.Chip(GPIO_CHIP)
        line = chip.get_line(GPIO_LINE)
        line.request(consumer="jetson-relay-switch", type=gpiod.LINE_REQ_DIR_OUT)
        line.set_value(target_value)
        return True
    except Exception as exc:
        print("Relay GPIO control error ({}:{}): {}".format(GPIO_CHIP, GPIO_LINE, exc), file=sys.stderr)
        return False


def main():
    info = get_pin_info()
    print("Relay Switch Pin Mapping (from environment):")
    print("  GPIO Chip   : {}".format(info["gpio_chip"]))
    print("  GPIO Line   : {}".format(info["gpio_line"]))
    print("  Pin Number  : {}".format(info["pin"]))
    print("  Active High : {}".format(info["active_high"]))

    if len(sys.argv) > 1:
        cmd = sys.argv[1].lower()
        if cmd == "on":
            if set_relay_state(True):
                print("Relay switch state -> ON")
        elif cmd == "off":
            if set_relay_state(False):
                print("Relay switch state -> OFF")


if __name__ == "__main__":
    main()
