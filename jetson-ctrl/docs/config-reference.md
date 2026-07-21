# `config.json` reference

The live config is `/etc/jetson-ctrl/config.json`. `config.example.json` in the
repo is the template — it is installed to `/usr/share/doc/jetson-ctrl/` as a
reference copy and **never overwrites** the live file.

**Hot-reloaded.** The daemon stats the file every tick and re-parses when the
mtime changes: edit and save, and the change takes effect within
`control_tick_ms`. No restart, no dropped cooling.

**A bad edit cannot break a running daemon.** Parse and validation happen into a
scratch copy; a failure logs `config: reload rejected, keeping previous good
config` and the last-good config keeps running (`config.cpp:120`). At *startup*
there is no previous good config, so a bad file is fatal — which is why you
bench-test in the foreground before enabling the service.

## Required vs optional

Five top-level objects are **required** — omit one and the parse fails:
`web_server`, `thermal`, `dht22`, `external_fan`, `control`.

Two are **optional**: `cadence` and `override`.

Every individual *field* is optional; anything absent keeps the compiled-in
default from `src/config.hpp`. So a minimal valid file is five empty objects —
though relying on defaults for the GPIO offsets is a bad idea, since they are
board-specific.

---

## `web_server`

Where telemetry goes and where fan overrides come from.

| Key | Default | Meaning |
|---|---|---|
| `base_url` | `http://localhost:3000` | The Node web-server. Telemetry POSTs to `<base_url>/api/v1/telemetry`; overrides come from `/api/v1/control`. |
| `device_id` | `jetson_ctrl_01` | Identity in the dashboard. Must match the ID used when sending an override, or the override never arrives. |
| `http_timeout_ms` | `2000` | Per-request timeout. Bounded deliberately: the network tick runs in the control loop, so a hung request would stall cooling decisions. |

Validation: `base_url` and `device_id` must be non-empty.

> Networking is **best-effort**. If the web-server is down the daemon logs a
> warning and keeps cooling — telemetry is a nice-to-have, cooling is not.

## `cadence` *(optional)*

How often things happen.

| Key | Default | Meaning |
|---|---|---|
| `control_tick_ms` | `2000` | The main loop period: read sensors, decide, actuate, feed the watchdog. |
| `network_every_ticks` | `5` | POST telemetry every Nth tick. At the defaults that is every 10 s. |
| `dht22_min_interval_ms` | `2000` | Floor between sensor reads. **Do not go below 2000** — the AM2302 will not answer faster and returns its previous sample. |

Validation: `control_tick_ms` ≥ 200, `network_every_ticks` ≥ 1.

`control_tick_ms` interacts with `WatchdogSec=15` in the unit file. The loop
feeds systemd's watchdog once per tick, so a tick longer than ~7 s would let
systemd kill a perfectly healthy daemon. Raise `WatchdogSec` if you ever want a
slow tick.

## `thermal`

On-die temperatures — the safety anchor.

| Key | Default | Meaning |
|---|---|---|
| `sysfs_root` | `/sys/class/thermal` | Where the kernel exposes thermal zones. No reason to change this. |
| `zone_names` | `["CPU-therm", "GPU-therm"]` | Zones to read, matched by their `type` file. The **hottest** becomes `zone_max_c`. |

Validation: `zone_names` must be non-empty.

List what your board actually has:

```bash
for z in /sys/class/thermal/thermal_zone*; do
  echo "$(cat $z/type): $(($(cat $z/temp)/1000))C"
done
```

Other Nano zones include `AO-therm`, `PLL-therm`, `thermal-fan-est`. Adding a
zone only ever makes the daemon *more* eager to cool, since the max is taken.

> **If no zone can be read, the fan is forced ON and `degraded` is flagged**
> (`control.cpp:11`). The daemon never guesses at a temperature.

## `dht22`

The enclosure-air sensor. Secondary — telemetry plus a second trigger.

| Key | Default | Meaning |
|---|---|---|
| `gpiochip` | `gpiochip0` | libgpiod chip name, not a path. |
| `line_offset` | `149` | Line for the DATA pin. **Board-specific — verify with `gpioinfo`.** 149 = header pin 29 on the Jetson Nano. |
| `max_retries` | `3` | Attempts per sampling round before giving up on that round. |
| `stale_after_ms` | `30000` | Age past which the last good reading stops counting as fresh. |

`stale_after_ms` is what makes a failing sensor safe. Once stale, `air_fresh`
goes false and the air trigger drops out of the decision entirely — the zone
trigger still protects the hardware. The reading stays in telemetry with
`enclosure_stale: true` so you can see it happening.

Sampling runs on a dedicated `SCHED_FIFO` thread because the protocol is
bit-banged with microsecond timing. If the daemon is not root you will see
`dht22: SCHED_FIFO unavailable` and reads will be less reliable.

## `external_fan`

The one GPIO the daemon writes.

| Key | Default | Meaning |
|---|---|---|
| `gpiochip` | `gpiochip0` | As above. |
| `line_offset` | `200` | Line driving the relay/MOSFET. 200 = header pin 31 on the Nano. |
| `active_high` | `true` | `true`: logical ON drives the line **high**. `false`: ON drives it **low** (active-low relay boards). |

**`active_high` must be bench-tested, not assumed.** The daemon's `external_fan:
"off"` in telemetry is its *logical* state; with the polarity inverted, "off"
means the relay is energised and the fan is running. Because `fan.cpp` is
fail-to-ON, that error looks like working hardware until the moment you need
cooling. Test with the daemon stopped:

```bash
sudo gpioset -m wait gpiochip0 200=1     # fan runs here => active_high: true
sudo gpioset -m wait gpiochip0 200=0     # fan runs here => active_high: false
```

If the line is held by another driver, the daemon fails at startup with
`EBUSY` naming the chip and offset — see
[host-setup.md](host-setup.md#gpio-allocation).

## `control`

The control law. Two independent triggers; the fan runs if **either** fires.

| Key | Default | Meaning |
|---|---|---|
| `temp_on_c` | `60.0` | Fan ON when `zone_max_c` reaches this. |
| `temp_off_c` | `52.0` | Fan OFF when it falls back below this. |
| `enclosure_air_on_c` | `45.0` | Fan ON when DHT22 air temp reaches this. |
| `enclosure_air_off_c` | `40.0` | Fan OFF when it falls back below this. |
| `min_on_seconds` | `30` | Minimum time ON before it may switch off. |
| `min_off_seconds` | `30` | Minimum time OFF before it may switch on. |

Validation: `temp_off_c` **must be** `< temp_on_c`, and likewise for the air
pair. `min_on/off_seconds` must be ≥ 0.

### Why two triggers

The die sensors protect the SoC but say nothing about the box. In a sealed
enclosure the air can cook the PSU, SD card or camera while the Jetson idles
cool — the air trigger catches that. A heavy AI workload spikes the die long
before the air responds — the zone trigger catches that. Each covers the
other's blind spot.

### Why the gap between on and off

The difference between `temp_on_c` and `temp_off_c` is a **deadband**. Without
it the fan would chatter on and off around a single threshold. Once running, it
keeps running until the temperature falls back past the *lower* number.

`min_on_seconds`/`min_off_seconds` are a second, time-based layer of the same
protection — relay contacts and fan bearings both dislike rapid cycling. A
consequence worth knowing during testing: after the fan starts, it stays on for
`min_on_seconds` even if you immediately raise the threshold back. That is the
dwell timer, not a stuck relay.

### Choosing thresholds

`temp_on_c` defaults to 60 °C, well under the Nano's ~97 °C throttle point —
this is the *enclosure* stage, not SoC protection, which NVIDIA's governor and
the built-in PWM fan already handle.

Set `enclosure_air_on_c` from the **least heat-tolerant part in the box**, not
the Nano. If a camera module or PSU is rated to 50 °C, 45 is sensible; if the
Nano is alone in there, you could go higher. Keep at least a few degrees of
deadband — 5 °C is a reasonable floor.

## `override` *(optional)*

Remote force-on from the dashboard.

| Key | Default | Meaning |
|---|---|---|
| `max_duration_minutes` | `120` | Ceiling on how long an override may last, whatever `until` the payload asks for. |

Validation: 1–240.

Send one with `POST /api/v1/control`:

```json
{ "external_fan_override": "force_on", "until": "2026-07-21T15:30:00Z" }
```

**Overrides are add-only.** `force_on` can start the fan early; there is no
`force_off`. An override can never turn the fan off below what the autonomous
decision wants, and the thermal safety trigger always wins (`control.cpp:33`).
The ceiling exists so a forgotten override cannot pin the fan on forever.

---

## Testing changes safely

The config hot-reloads, so a threshold can be exercised without a restart:

```bash
# force the fan on (current die temp is ~23 C)
sudo sed -i 's/"temp_on_c": 60.0/"temp_on_c": 20.0/' /etc/jetson-ctrl/config.json
# put it back
sudo sed -i 's/"temp_on_c": 20.0/"temp_on_c": 60.0/' /etc/jetson-ctrl/config.json
```

Watch the effect:

```bash
journalctl -u jetson-ctrl -f
curl -s localhost:3000/api/v1/devices | python3 -m json.tool
```

The `reason` field in telemetry says which trigger decided: `below thresholds`,
`zone above threshold`, `enclosure air above threshold`, `zone+air above
threshold`, `override force_on`, or `degraded: thermal unreadable -> forced ON`.

Validate JSON before saving, since a syntax error is silently rejected at
runtime (and fatal at startup):

```bash
python3 -m json.tool < /etc/jetson-ctrl/config.json > /dev/null && echo OK
```
