# Sensor Parity Procedure — legacy ESP-IDF vs. new PlatformIO

This document describes how we verify that the PlatformIO port of the sensor
node produces the **same physical-pin readings** (temperature, humidity, soil
moisture) as the legacy ESP-IDF firmware it replaces.

## What parity does and does NOT cover

**In scope (guaranteed):** the raw sensor values coming off the physical pins —
the DHT22 one-wire decode (GPIO 32) and the soil-moisture ADC read + mapping
(ADC1 ch6). These are compared upstream at the serial log, *before* the network
transport, so the check isolates GPIO/ADC/DHT behavior from anything else.

**Out of scope (intentionally different):** the HTTP transport and JSON body.
The port deliberately changed both:

| | Legacy (ESP-IDF) | New (PlatformIO) |
|---|---|---|
| Endpoint | `http://192.168.0.1:80/sensor-update` | `http://192.168.0.2:3000/api/v1/telemetry` |
| Body shape | flat: `{"device_id":...,"temperature":T,"humidity":H,"soil_moisture":S}` | metrics-nested: `{"device_id":...,"metrics":{"temperature":T,"humidity":H,"soil_moisture":S}}` |
| Timestamp | (none) | omitted by device; server stamps it |

So **HTTP-body parity is NOT expected and must not be asserted.** The new body
shape is locked separately by the native unit test `test/test_json_body/`
(it matches the web-server dashboard's `metricMeta` contract). The parity
harness here only diffs the *readings*.

## Why serial, not the server

Both firmwares print, once per reading, a `sensor_task` line of the form:

```
I (12345) sensor_task: [7] temp=27.3°C  humidity=61.0%  soil=44.2%
```

Because this line is emitted from the same point in both codebases (right after
the sensor reads, before the POST), diffing it compares apples-to-apples without
either server being involved. A server-side comparison would conflate the
transport change with the reading logic.

## Procedure

### 1. Flash both binaries onto the SAME board under identical conditions

Use one physical board and do not disturb the sensors or the environment between
runs. The DHT22 and soil probe must stay in place so both firmwares see the same
physical stimulus.

Legacy (from `esp-idf-iot/sensor-node/`):

```
idf.py build flash
```

New (from `sensor-zone/`):

```
pio run -t upload
```

Flash one, capture its log (below), then flash the other and capture again.
Keep both capture windows a comparable length (same number of readings).

### 2. Capture both serial logs

New build (PlatformIO):

```
pio device monitor | tee new.log
# let it run for N readings, then Ctrl-C
```

Legacy build (ESP-IDF):

```
idf.py monitor | tee legacy.log
# let it run for N readings, then Ctrl-C
```

Any capture that preserves stdout works (`> file.log`, `| tee file.log`, a
terminal-emulator log, etc.). The harness is tolerant of the ESP-IDF log prefix
and of the `°C` / `%` unicode suffixes.

### 3. Run the diff

```
python tools/parity_diff.py legacy.log new.log
```

Tolerances can be overridden:

```
python tools/parity_diff.py legacy.log new.log --temp-tol 0.2 --hum-tol 1.0 --soil-tol 1.0
```

### 4. Interpret the results

The harness aligns readings by their `[n]` index (last occurrence wins if an
index repeats), then per metric reports `max|d|`, `mean|d|`, the tolerance, and
the number of samples that exceeded it. Example PASS:

```
legacy readings: 20   new readings: 20   aligned by index: 20

metric         max|d|  mean|d|      tol  fails
----------------------------------------------
temperature     0.100    0.050    0.200      0
humidity        0.200    0.100    1.000      0
soil            0.100    0.067    1.000      0

PARITY: PASS - all aligned readings within tolerance.
```

Exit code:

- `0` — PASS, every aligned metric within tolerance.
- `1` — FAIL, at least one metric exceeded tolerance (offending samples are
  listed with legacy/new/delta).
- `2` — usage/parse error (a log unreadable, no matching lines, or no
  overlapping reading indices — usually a mismatched counter, so re-capture both
  with the counter reset, i.e. reset both boards before capturing).

If FAIL: confirm the sensors were physically undisturbed between runs, that the
reading indices actually overlap (reset both boards so both start at `[1]`), and
that the environment (temperature/moisture) was stable during both captures.
Small transient deltas on a single reading usually mean the physical value moved
between the two capture windows — re-run with steadier conditions before
suspecting a code regression.

## Tolerances and why

| Metric | Default tol | Rationale |
|---|---|---|
| temperature | 0.2 °C | The serial line prints `%.1f`, so readings quantize to 0.1 °C; 0.2 °C absorbs one quantization step plus DHT22's own ±0.5 °C sensor jitter between the two non-simultaneous capture windows. |
| humidity | 1.0 % | DHT22 humidity is noisier (±2–5 % RH accuracy) and drifts between runs; 1.0 % catches a logic/scaling regression while tolerating normal RH wander. |
| soil | 1.0 % | Soil percent comes from a 16-sample ADC average mapped through the same `map_voltage_to_percent`; 1.0 % tolerates ADC noise/averaging differences while flagging a mapping or calibration-constant regression. |

These are the parity-check tolerances (defaults in `tools/parity_diff.py`), not
sensor spec limits — they are deliberately tight enough to catch a code
regression in the decode/mapping path but loose enough to survive the fact that
the legacy and new logs are captured at different moments in time.
