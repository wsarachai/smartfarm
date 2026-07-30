#!/usr/bin/env python3
"""
Relay Switch / External Fan Controller interface for Edge Devices.
Reads pin mapping from environment variables (.env / env_config).
Supports gpiod v1, gpiod v2, gpioset CLI, and sysfs GPIO fallbacks across Jetson and Raspberry Pi.
"""
import glob
import os
import subprocess
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


def set_relay_with_gpiod(chip_path, line_offset, target_value):
    """Tries controlling GPIO line using Python gpiod library (v1 or v2)."""
    try:
        import gpiod
    except ImportError:
        return False, "gpiod module not installed"

    # Attempt gpiod v1 API (gpiod 1.x)
    try:
        chip = gpiod.Chip(chip_path)
        if hasattr(chip, "get_line"):
            line = chip.get_line(line_offset)
            try:
                line.request(consumer="edge-relay", type=gpiod.LINE_REQ_DIR_OUT)
            except (TypeError, AttributeError):
                line.request("edge-relay", gpiod.LINE_REQ_DIR_OUT)
            line.set_value(target_value)
            chip.close()
            return True, "gpiod v1 ({})".format(chip_path)
        chip.close()
    except Exception as e1:
        pass

    # Attempt gpiod v2 API (gpiod 2.x)
    try:
        if hasattr(gpiod, "request_lines"):
            val_enum = getattr(gpiod.line, "Value", None) if hasattr(gpiod, "line") else None
            active_val = getattr(val_enum, "ACTIVE", 1) if val_enum else 1
            inactive_val = getattr(val_enum, "INACTIVE", 0) if val_enum else 0
            val_to_set = active_val if target_value == 1 else inactive_val

            dir_enum = getattr(gpiod.line, "Direction", None) if hasattr(gpiod, "line") else None
            out_dir = getattr(dir_enum, "OUTPUT", None) if dir_enum else None

            settings = gpiod.LineSettings(direction=out_dir, output_value=val_to_set)
            req = gpiod.request_lines(chip_path, consumer="edge-relay", config={line_offset: settings})
            req.set_value(line_offset, val_to_set)
            req.release()
            return True, "gpiod v2 ({})".format(chip_path)
    except Exception as e2:
        pass

    return False, "gpiod API incompatible or failed on {}".format(chip_path)


def set_relay_with_gpioset(chip_path, line_offset, target_value):
    """Tries controlling GPIO line using gpioset CLI utility."""
    chip_base = os.path.basename(chip_path)
    chip_num = chip_base.replace("gpiochip", "") if chip_base.startswith("gpiochip") else chip_base

    commands = [
        ["gpioset", chip_path, "{}={}".format(line_offset, target_value)],
        ["gpioset", "-c", chip_path, "{}={}".format(line_offset, target_value)],
        ["gpioset", chip_base, "{}={}".format(line_offset, target_value)],
        ["gpioset", "-c", chip_num, "{}={}".format(line_offset, target_value)],
        ["gpioset", chip_num, "{}={}".format(line_offset, target_value)],
    ]

    for cmd in commands:
        try:
            res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
            if res.returncode == 0:
                return True, "gpioset CLI ({})".format(" ".join(cmd))
        except Exception:
            continue

    return False, "gpioset CLI failed"


def set_relay_with_sysfs(line_offset, target_value):
    """Tries controlling GPIO line using legacy sysfs interface (/sys/class/gpio)."""
    try:
        sysfs_gpio_path = "/sys/class/gpio/gpio{}".format(line_offset)
        if not os.path.exists(sysfs_gpio_path):
            with open("/sys/class/gpio/export", "w") as f:
                f.write(str(line_offset))
        with open("{}/direction".format(sysfs_gpio_path), "w") as f:
            f.write("out")
        with open("{}/value".format(sysfs_gpio_path), "w") as f:
            f.write(str(target_value))
        return True, "sysfs (gpio{})".format(line_offset)
    except Exception as exc:
        return False, "sysfs error: {}".format(exc)


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
    """Prints detailed diagnostic information about available GPIO chips on host."""
    print("==================================================================")
    print(" GPIO System Diagnostics")
    print("==================================================================")
    available_nodes = sorted(glob.glob("/dev/gpiochip*"))
    print("Available /dev/gpiochip* nodes: {}".format(available_nodes if available_nodes else "None found"))

    try:
        import gpiod
        gpiod_version = getattr(gpiod, "__version__", "v1/legacy")
        print("gpiod Python module version: {}".format(gpiod_version))

        for cand in available_nodes:
            try:
                c = gpiod.Chip(cand)
                label = getattr(c, "label", "unknown")
                if callable(label):
                    label = label()
                num_l = getattr(c, "num_lines", "unknown")
                if callable(num_l):
                    num_l = num_l()
                print("  Node {:<18} -> label: {:<20} lines: {}".format(cand, str(label), str(num_l)))
                c.close()
            except Exception as exc:
                print("  Node {:<18} -> error opening: {}".format(cand, exc))
    except ImportError:
        print("gpiod Python module is NOT installed.")

    print("==================================================================")


def set_relay_state(state_on):
    """
    Set Relay Switch state ON (True) or OFF (False).
    Evaluates all candidate chip paths using gpiod (v1/v2), gpioset CLI, and sysfs.
    """
    target_value = (1 if ACTIVE_HIGH else 0) if state_on else (0 if ACTIVE_HIGH else 1)
    state_label = "ON" if state_on else "OFF"
    candidates = get_candidate_chip_paths(GPIO_CHIP)

    # 1. Try Python gpiod (v1 or v2) across candidate chips
    for chip_path in candidates:
        ok, msg = set_relay_with_gpiod(chip_path, GPIO_LINE, target_value)
        if ok:
            print("Relay set to {} via {}".format(state_label, msg))
            return True

    # 2. Try gpioset CLI across candidate chips
    for chip_path in candidates:
        ok, msg = set_relay_with_gpioset(chip_path, GPIO_LINE, target_value)
        if ok:
            print("Relay set to {} via {}".format(state_label, msg))
            return True

    # 3. Try legacy sysfs GPIO
    ok, msg = set_relay_with_sysfs(GPIO_LINE, target_value)
    if ok:
        print("Relay set to {} via {}".format(state_label, msg))
        return True

    print("Relay GPIO control failed for line {}. Diagnostics below:".format(GPIO_LINE), file=sys.stderr)
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
