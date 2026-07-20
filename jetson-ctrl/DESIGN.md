# jetson-ctrl — Design

A C++17 **host daemon** for the Jetson Nano (Ubuntu 18.04 / L4T) that manages the
enclosure's **external fan** as a *second cooling stage*, reports thermal state to
the existing SmartFarm web-server, and self-heals under systemd.

> This project is categorically different from the rest of the repo: it is **not**
> ESP firmware (`sensor-zone`, `pump-zone`, …) and **not** the Node web-server. It
> is a native Linux daemon that runs on the Jetson that *hosts* the web-server.

---

## Problem

The Jetson lives in a sealed box that retains heat. NVIDIA's thermal governor
already drives the Nano's **built-in 4-pin PWM fan** to protect the SoC junction,
but it knows nothing about *enclosure* airflow. We add an **external fan** (GPIO
on/off) that engages when the box is still too hot, and we surface the box's
thermal health on the dashboard we already run.

---

## Decision record

Each decision below was resolved deliberately (see the `Q#` tags). The through-line
is **safety first**: cooling must be autonomous, local, and fail toward *more*
cooling — never dependent on the network, and never something a remote command can
weaken.

### Role in the ecosystem
- **Q1 — Full node.** Reports telemetry **and** honors a remote override.
- **Q2 — Pure HTTP client, no inbound port.** POSTs telemetry to
  `POST /api/v1/telemetry`; reads overrides by polling `GET /api/v1/devices` and
  looking at its own device's `lastCommand` (**desired-state reconciliation** — the
  web-server has no command queue; `applyCommand` just merges the action into the
  device and stores `lastCommand`). **No web-server changes are required.**

### Sensing
- **Q4 — Two-tier temperature.** The **control anchor is the Jetson's on-die
  thermal zones** (reliable, never bit-banged). The **DHT22 measures enclosure air**
  and is a **secondary + telemetry-only** input; a stale/failed DHT22 read *never*
  affects cooling.
- **Q12 — Zones resolved by name.** Read each `thermal_zone*/type`, control on
  `max(allowlist)`, default allowlist `["CPU-therm","GPU-therm"]`. Zone **indices
  reorder across L4T kernels**, so we never trust `thermal_zoneN` by index.
- **Q5 — DHT22 = fixed hardware.** Bit-banged via **libgpiod** in a dedicated
  `SCHED_FIFO` + `mlockall` sampling thread, ≥2 s cadence (DHT22 caps at 0.5 Hz),
  N retries, keeping a **last-good value + timestamp** and a **stale** flag.

### Actuation
- **Q6 — The daemon writes exactly ONE GPIO: the external fan.** The **built-in PWM
  fan stays owned by NVIDIA's governor** (untouched safety net underneath; we only
  *read* `target_pwm` for telemetry). The external fan is the second stage.

### Control law
- **Q7 — Bang-bang with hysteresis + minimum dwell.** External fan ON when
  `max(thermal zones) ≥ temp_on_c` **OR** (DHT22 valid **and**
  `enclosure_air ≥ enclosure_air_on_c`); OFF only when **both** are back under their
  respective `*_off_c` points. `min_on_seconds` / `min_off_seconds` protect the
  relay/fan from chatter. The thermal-zone trigger is always present even when the
  DHT22 is stale.
- All thresholds/hysteresis/dwell/zone-list are **hot-reloaded** from config by
  file mtime, and a bad edit falls back to the **last-known-good** config (a
  fat-fingered edit must never crash or blank the cooling loop).

### Remote override — safety envelope
- **Q8a — Verbs: `auto` + `force_on` ONLY.** There is **no force-off verb.** The
  worst a forgotten/compromised override can do is run the fan when it wasn't
  strictly needed. This removes an entire class of hazard by construction.
- **Q8b — Absolute expiry.** The override payload carries an absolute ISO-8601
  `until`; it is honored only while `now < until`, then auto-reverts to autonomous.
  Absolute time (not a relative TTL) so it survives daemon restarts. Default short
  (~15 min), hard-capped by `override.max_duration_minutes` (~2 h).
  - *Why explicit expiry is mandatory:* the web-server store can't age `lastCommand`
    — the daemon bumps the device's `lastSeen` with its own telemetry POSTs — so the
    daemon can't infer command age from the store.
- **Invariant — the autonomous thermal-zone safety trigger always wins.** An
  override may **add** cooling, never remove it.

### Fail-safe
- **Q10 — Fail-to-cooling.**
  - No valid thermal-zone temperature (path missing/unreadable/garbage) → **force
    fan ON** + a `degraded` flag in telemetry. Never idle at unknown-temp-fan-off.
  - **Boot initializes the fan ON**, then the first valid reading settles it.
  - GPIO write failure → log, retry, surface `degraded`.
  - On crash, the relay **holds its last hardware state**, and NVIDIA's governor is
    still the untouched SoC backstop.

### Deployment
- **Q9 — C++17**, `libcurl` (HTTP) + `nlohmann/json` (vendored header) + `libgpiod`
  (DHT22 + fan), **CMake**. RAII for leak-free long-running resource management.
- **Q11 — systemd, root (v1).** `Restart=always`, `RestartSec=2`,
  **`WatchdogSec` + `sd_notify(WATCHDOG=1)`** pinged each tick (catches a *hung*
  loop, the scariest failure), **no network gating** (`After=local-fs.target`,
  `WantedBy=multi-user.target`, starts early — cooling must begin at boot regardless
  of network).
- **Q13 — Cadence & config.** Control loop **~2 s tick**; telemetry POST + override
  GET **~10 s** (every 5th tick), best-effort with **short libcurl timeouts that
  never block the control tick**. Live config at **`/etc/jetson-ctrl/config.json`**;
  `config.example.json` committed in-repo.

---

## Architecture

```
                 ┌────────────────────────── jetson-ctrl (root, systemd) ─────────────────────────┐
                 │                                                                                 │
  /sys/class/thermal ──read──►  thermal.cpp  (resolve zones by name, max)                          │
                 │                     │                                                           │
  DHT22 (libgpiod, GPIO) ─►  dht22.cpp (SCHED_FIFO sampling thread → last-good + stale)            │
                 │                     │            │                                              │
                 │                     ▼            ▼                                              │
                 │                  control.cpp  (bang-bang + hysteresis + dwell + override gate)  │
                 │                     │                                                           │
  external fan (libgpiod) ◄─write──  fan.cpp  (min on/off dwell, fail-to-ON)                        │
                 │                     │                                                           │
                 │                  net.cpp  (libcurl; POST telemetry / GET override) ── best effort│
                 │                     │                                                           │
                 │                  notify.hpp (sd_notify WATCHDOG=1 each tick)                    │
                 └─────────────────────┼───────────────────────────────────────────────────────────┘
                                       ▼
                     Node web-server  POST /api/v1/telemetry  ·  GET /api/v1/devices
```

### Control loop (main.cpp), ~2 s tick
1. `sd_notify(WATCHDOG=1)`.
2. Reload config if its mtime changed (validate; keep last-good on error).
3. Read thermal zones → `max_zone_c` (or `degraded` if unreadable).
4. Read cached DHT22 (`temp`, `humidity`, `valid`/`stale`).
5. Resolve the **effective override** (`force_on` iff `now < until`, else `auto`).
6. `control::decide(...)` → desired fan state, honoring hysteresis, dwell, the
   override (add-only), and the **fail-to-cooling** rule.
7. Drive the fan (`fan::set`), respecting `min_on/off` dwell.
8. Every 5th tick: POST telemetry, GET override — best-effort, short timeout.

### Threads
- **main** — the control loop above; single writer of the fan GPIO.
- **dht22 sampler** — `SCHED_FIFO`, `mlockall`, bit-bangs the DHT22 at ≥2 s and
  publishes a last-good snapshot behind a mutex. Timing-sensitive work is isolated
  here so scheduler jitter can't stall the control cadence.

---

## Config schema (`/etc/jetson-ctrl/config.json`)

See `config.example.json`. Notable fields:

| Key | Meaning |
|-----|---------|
| `web_server.base_url` | e.g. `http://localhost:3000` (co-located Node server). |
| `web_server.device_id` | `jetson_ctrl_01` — identity on both telemetry & override paths. |
| `thermal.zone_names` | Allowlist resolved by `type`; control on their **max**. |
| `dht22.gpiochip` / `line_offset` | libgpiod chip + line for the DHT22 data pin. |
| `external_fan.gpiochip` / `line_offset` / `active_high` | The one actuated line. |
| `control.temp_on_c` / `temp_off_c` | Thermal-zone hysteresis (deadband). |
| `control.enclosure_air_on_c` / `enclosure_air_off_c` | DHT22 OR-trigger deadband. |
| `control.min_on_seconds` / `min_off_seconds` | Relay chatter protection. |
| `override.max_duration_minutes` | Hard cap on any override's `until`. |

> **GPIO line offsets are placeholders.** On the Jetson, map the physical 40-pin
> header pin to its `gpiochip` line with `sudo gpioinfo` / `gpiofind`, then set
> `line_offset` accordingly. JSON has no comments — document your pin choice here.

---

## Telemetry payload

```json
{
  "device_id": "jetson_ctrl_01",
  "metrics": {
    "zone_max_c": 61.5,
    "cpu_therm_c": 61.5,
    "gpu_therm_c": 58.0,
    "enclosure_temp_c": 44.2,
    "enclosure_humidity": 63.1,
    "enclosure_stale": false,
    "builtin_fan_pwm": 168,
    "external_fan": "on",
    "override": "auto",
    "degraded": false
  }
}
```
(Timestamp omitted — the server stamps `lastSeen`.)

## Override payload (set by the dashboard via `POST /api/v1/control`)

```json
{ "device_id": "jetson_ctrl_01",
  "action": { "external_fan_override": "force_on", "until": "2026-07-20T15:30:00Z" } }
```
`external_fan_override` ∈ {`auto`, `force_on`}. `force_on` honored only while
`now < until` and `until - now ≤ override.max_duration_minutes`.

---

## Known risks / verify on the real box

1. **DHT22 timing** is the one soft spot; the design routes *safety* entirely around
   it (thermal zones drive cooling). If the bench drop-rate is intolerable even for
   telemetry, swapping to a kernel `dht11` overlay or an I²C BME280 changes **no
   other decision** — that's the payoff of anchoring control to the thermal zones.
2. **Governor backstop assumption (Q6).** The daemon intentionally does **not**
   protect the SoC junction — it relies on NVIDIA's governor. Before trusting this,
   confirm on the actual L4T image that `nvfancontrol` / the fan cooling-device is
   active (`systemctl status nvfancontrol`; `cat /sys/devices/pwm-fan/target_pwm`
   should rise under load).
3. **GPIO line offsets** must be mapped to your real header pins before first run.
