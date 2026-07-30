#!/usr/bin/env python3
"""
Relay Switch / External Fan Controller interface for Edge Devices.
Reads pin mapping from environment variables (.env / env_config).
Auto-detects available /dev/gpiochip* devices on Raspberry Pi and Jetson.
"""
import glob
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


def find_working_chip(requested_chip, line_offset):
    """
    Finds an accessible gpiod.Chip instance that contains line_offset.
    Checks requested_chip, /dev/<chip>, and all /dev/gpiochip* nodes.
    """
    import gpiod

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
            num_lines_attr = getattr(chip, "num_lines", None)
            if callable(num_lines_attr):
                total_lines = num_lines_attr()
            elif isinstance(num_lines_attr, int):
                total_lines = num_lines_attr
            else:
                total_lines = 512

            if line_offset < total_lines:
                try:
                    line = chip.get_line(line_offset)
                    if line:
                        return chip, cand
                except Exception:
                    pass

            chip.close()
        except Exception as exc:
            last_error = exc
            continue

    raise RuntimeError("No accessible GPIO chip found for line {}. Errors: {}".format(line_offset, last_error))


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
    Uses libgpiod with auto chip resolution, falling back to gpioset CLI or sysfs.
    """
    target_value = (1 if ACTIVE_HIGH else 0) if state_on else (0 if ACTIVE_HIGH else 1)

    # Method 1: Try Python gpiod with chip auto-detection
    try:
        import gpiod
        chip_obj, resolved_name = find_working_chip(GPIO_CHIP, GPIO_LINE)
        line = chip_obj.get_line(GPIO_LINE)
        try:
            line.request(consumer="edge-relay-switch", type=gpiod.LINE_REQ_DIR_OUT)
        except (TypeError, AttributeError):
            line.request("edge-relay-switch", gpiod.LINE_REQ_DIR_OUT)
        
        line.set_value(target_value)
        print("Relay set to {} via gpiod ({}:line {})".format("ON" if state_on else "OFF", resolved_name, GPIO_LINE))
        return True
    except Exception as exc1:
        # Method 2: Try gpioset CLI utility
        try:
            import subprocess
            chip_names = [GPIO_CHIP, "/dev/" + os.path.basename(GPIO_CHIP)] + sorted(glob.glob("/dev/gpiochip*"))
            for c in chip_names:
                if os.path.exists(c) or os.path.exists("/dev/" + os.path.basename(c)):
                    res = subprocess.run(["gpioset", c, "{}={}".format(GPIO_LINE, target_value)],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    if res.returncode == 0:
                        print("Relay set to {} via gpioset ({}:line {})".format("ON" if state_on else "OFF", c, GPIO_LINE))
                        return True
        except Exception:
            pass

        # Method 3: Try sysfs GPIO (/sys/class/gpio)
        try:
            sysfs_gpio_path = "/sys/class/gpio/gpio{}".format(GPIO_LINE)
            if not os.path.exists(sysfs_gpio_path):
                with open("/sys/class/gpio/export", "w") as f:
                    f.write(str(GPIO_LINE))
            with open("{}/direction".format(sysfs_gpio_path), "w") as f:
                f.write("out")
            with open("{}/value".format(sysfs_gpio_path), "w") as f:
                f.write(str(target_value))
            print("Relay set to {} via sysfs (gpio{})".format("ON" if state_on else "OFF", GPIO_LINE))
            return True
        except Exception:
            pass

        print("Relay GPIO control error ({}:{}): {}".format(GPIO_CHIP, GPIO_LINE, exc1), file=sys.stderr)
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
            set_relay_state(True)
        elif cmd == "off":
            set_relay_state(False)


if __name__ == "__main__":
    main()
