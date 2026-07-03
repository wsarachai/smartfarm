#!/usr/bin/env python3
"""
parity_diff.py — legacy-vs-new sensor telemetry parity harness.

Diffs the per-reading sensor stream of the LEGACY ESP-IDF binary against the
NEW PlatformIO binary, flashed onto the SAME board under identical physical
conditions. This isolates GPIO/ADC/DHT parity from the deliberate
transport/body change (endpoint + JSON shape changed on purpose — that is NOT
what we verify here; we verify the physical-pin readings upstream at serial).

Both firmwares emit, from sensor_task, a line like:

    [<n>] temp=<..>°C  humidity=<..>%  soil=<..>%

e.g.  I (12345) sensor_task: [7] temp=27.3°C  humidity=61.0%  soil=44.2%

How to capture the two logs
---------------------------
NEW build (PlatformIO), from sensor-zone/:
    pio device monitor | tee new.log
    # ...let it run for N readings, then Ctrl-C...

LEGACY build (ESP-IDF), from esp-idf-iot/sensor-node/:
    idf.py monitor | tee legacy.log
    # (or:  idf.py monitor > legacy.log  — anything that captures stdout)

Flash each firmware in turn onto the SAME board, leave the sensors untouched
between runs, and capture a comparable number of readings for each.

Run
---
    python tools/parity_diff.py legacy.log new.log
    python tools/parity_diff.py legacy.log new.log --temp-tol 0.2 --hum-tol 1.0 --soil-tol 1.0

Exit code is 0 (PASS) if every aligned metric is within tolerance, else 1 (FAIL).
"""

import argparse
import re
import sys

# Tolerant of the °C / % unicode suffixes and variable whitespace.
# Captures: index, temp, humidity, soil.
LINE_RE = re.compile(
    r"\[(\d+)\]\s*temp=([\-\d.]+).*?humidity=([\-\d.]+).*?soil=([\-\d.]+)"
)

METRICS = ("temperature", "humidity", "soil")


def parse_log(path):
    """Return {index: {'temperature':float,'humidity':float,'soil':float}}.

    If an index repeats (counter wrapped, or two capture sessions concatenated),
    the LAST occurrence wins.
    """
    readings = {}
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                m = LINE_RE.search(line)
                if not m:
                    continue
                idx = int(m.group(1))
                readings[idx] = {
                    "temperature": float(m.group(2)),
                    "humidity": float(m.group(3)),
                    "soil": float(m.group(4)),
                }
    except OSError as exc:
        sys.stderr.write("ERROR: cannot read %s: %s\n" % (path, exc))
        sys.exit(2)
    return readings


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Diff legacy vs new sensor telemetry serial logs by reading index."
    )
    ap.add_argument("legacy_log", help="captured serial log from the LEGACY ESP-IDF build")
    ap.add_argument("new_log", help="captured serial log from the NEW PlatformIO build")
    ap.add_argument("--temp-tol", type=float, default=0.2, help="temperature tolerance degC (default 0.2)")
    ap.add_argument("--hum-tol", type=float, default=1.0, help="humidity tolerance %% (default 1.0)")
    ap.add_argument("--soil-tol", type=float, default=1.0, help="soil tolerance %% (default 1.0)")
    args = ap.parse_args(argv)

    tol = {"temperature": args.temp_tol, "humidity": args.hum_tol, "soil": args.soil_tol}

    legacy = parse_log(args.legacy_log)
    new = parse_log(args.new_log)

    if not legacy:
        sys.stderr.write("ERROR: no sensor lines matched in %s\n" % args.legacy_log)
        sys.exit(2)
    if not new:
        sys.stderr.write("ERROR: no sensor lines matched in %s\n" % args.new_log)
        sys.exit(2)

    common = sorted(set(legacy) & set(new))
    only_legacy = sorted(set(legacy) - set(new))
    only_new = sorted(set(new) - set(legacy))

    print("legacy readings: %d   new readings: %d   aligned by index: %d"
          % (len(legacy), len(new), len(common)))
    if only_legacy:
        print("  (indices only in legacy: %s)" % _fmt_idx(only_legacy))
    if only_new:
        print("  (indices only in new:    %s)" % _fmt_idx(only_new))

    if not common:
        sys.stderr.write("ERROR: no overlapping reading indices to compare.\n")
        sys.exit(2)

    # Per-metric stats + violations.
    stats = {m: {"max": 0.0, "sum": 0.0, "n": 0, "viol": []} for m in METRICS}
    for idx in common:
        for m in METRICS:
            delta = abs(new[idx][m] - legacy[idx][m])
            s = stats[m]
            s["sum"] += delta
            s["n"] += 1
            if delta > s["max"]:
                s["max"] = delta
            if delta > tol[m]:
                s["viol"].append((idx, legacy[idx][m], new[idx][m], delta))

    print("")
    print("%-12s %8s %8s %8s %6s" % ("metric", "max|d|", "mean|d|", "tol", "fails"))
    print("-" * 46)
    failed = False
    for m in METRICS:
        s = stats[m]
        mean = s["sum"] / s["n"] if s["n"] else 0.0
        n_viol = len(s["viol"])
        if n_viol:
            failed = True
        print("%-12s %8.3f %8.3f %8.3f %6d"
              % (m, s["max"], mean, tol[m], n_viol))

    # Detail on violations.
    for m in METRICS:
        for idx, lv, nv, delta in stats[m]["viol"]:
            print("  FAIL %-11s [%d] legacy=%.3f new=%.3f d=%.3f (tol %.3f)"
                  % (m, idx, lv, nv, delta, tol[m]))

    print("")
    if failed:
        print("PARITY: FAIL - one or more metrics exceeded tolerance.")
        return 1
    print("PARITY: PASS - all aligned readings within tolerance.")
    return 0


def _fmt_idx(indices, limit=12):
    if len(indices) <= limit:
        return ", ".join(str(i) for i in indices)
    head = ", ".join(str(i) for i in indices[:limit])
    return "%s, ... (+%d more)" % (head, len(indices) - limit)


if __name__ == "__main__":
    sys.exit(main())
