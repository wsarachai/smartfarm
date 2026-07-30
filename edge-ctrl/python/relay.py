#!/usr/bin/env python3
"""
Relay Switch / External Fan Controller interface for Edge Devices.
Reads pin mapping from environment variables (.env / env_config).
Auto-detects available GPIO chips across Linux devices (Jetson, RPi 3B, RPi 4, RPi 5).
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
    Tries integer chip numbers, device paths (/dev/gpiochip*), and chip names.
    """
    import gpiod

    candidates = []

    # If requested_chip is e.g. "gpiochip0" or "0", include integer 0
    req_str = str(requested_chip)
    if req_str.isdigit():
        candidates.append(int(req_str))
    elif req_str.startswith("gpiochip") and req_str[8:].isdigit():
        candidates.append(int(req_str[8:]))

    candidates.append(requested_chip)

    if not req_str.startswith("/"):
        candidates.append("/dev/" + os.path.basename(req_str))

    # Add all existing /dev/gpiochip* nodes
    for c in sorted(glob.glob("/dev/gpiochip*")):
        if c not in candidates:
            candidates.append(c)

    # Add fallback integer indices 0..9
    for i in range(10):
        if i not in candidates:
            candidates.append(i)

    errors = []
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
                        return chip, str(cand)
                except Exception as line_err:
                    chip.close()
                    errors.append("{}: get_line({}) failed: {}".format(cand, line_offset, line_err))
                    continue

            chip.close()
        except Exception as chip_err:
            errors.append("{}: {}".format(cand, chip_err))
            continue

    raise RuntimeError("No accessible GPIO chip found for line {}. Errors: {}".format(line_offset, "; ".join(errors[:4])))


def get_pin_info():
    """Returns configured pin mapping details for Relay Switch."""
    return {
        "switch": "Relay",
        "gpio_chip": GPIO_CHIP,
        "gpio_line": GPIO_LINE,
        "pin": PIN,
        "active_high": ACTIVE_HIGH
    }


def diagnose_gpio():
    """Prints diagnostic information about available GPIO chips on host."""
    print("==================================================================")
    print(" GPIO Diagnostics")
    print("==================================================================")
    print("Available /dev/gpiochip* nodes: {}".format(glob.glob("/dev/gpiochip*")))

    try:
        import gpiod
        print("gpiod module loaded successfully.")
        for cand in sorted(glob.glob("/dev/gpiochip*")) + [0, 1, 2, 3, 4]:
            try:
                c = gpiod.Chip(cand)
                label = getattr(c, "label", "unknown")
                if callable(label):
                    label = label()
                num_l = getattr(c, "num_lines", "unknown")
                if callable(num_l):
                    num_l = num_l()
                print("  Chip {:<15} -> label: {:<20} lines: {}".format(str(cand), str(label), str(num_l)))
                c.close()
            except Exception as exc:
                print("  Chip {:<15} -> error: {}".format(str(cand), exc))
    except ImportError:
        print("gpiod module is NOT installed (sudo apt install python3-gpiod gpiod)")
    print("==================================================================")


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
        diagnose_gpio()
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
        elif cmd in ("--diag", "diag", "status"):
            diagnose_gpio()


if __name__ == "__main__":
    main()
