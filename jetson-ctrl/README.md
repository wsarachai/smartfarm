# jetson-ctrl

Host daemon that runs the SmartFarm enclosure's **external fan** as a second
cooling stage on the Jetson Nano, reports thermal telemetry to the Node
web-server, and self-heals under systemd. See **[DESIGN.md](DESIGN.md)** for the
full rationale and decision record.

> Not ESP firmware and not the Node server — this is a native C++17 Linux daemon
> that runs on the Jetson itself.

## Layout

```
jetson-ctrl/
├── src/ tests/ third_party/   # the C++17 daemon (this README)
├── CMakeLists.txt
├── systemd/jetson-ctrl.service
├── config.example.json
├── python/                    # host provisioning: DS3231 RTC timekeeping
└── docs/host-setup.md         # wiring, GPIO map, RTC units, troubleshooting
```

The **[`python/`](python/)** tree is separate on purpose — it is one-shot host
setup that runs at boot and exits, not part of the control loop. Anything about
the machine underneath the daemon (I²C RTC, 40-pin header allocation, boot
units) lives in **[docs/host-setup.md](docs/host-setup.md)**.

## What it controls
- **Reads:** on-die thermal zones (`/sys/class/thermal`, the safety anchor) + a
  DHT22 enclosure-air sensor (secondary/telemetry).
- **Writes:** exactly one GPIO — the external fan relay/MOSFET. The Nano's
  built-in PWM fan is left to NVIDIA's governor.

## Build (on the Jetson)

```bash
sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev libgpiod-dev
# fetch the JSON header (see third_party/nlohmann/README.md), or install nlohmann-json3-dev
curl -L -o third_party/nlohmann/json.hpp \
  https://github.com/nlohmann/json/releases/latest/download/json.hpp

mkdir -p build && cd build
cmake ..
make -j$(nproc)

# for debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

> **CMake on JetPack 4.x is 3.10.2**, which is old enough to matter — the daemon
> is compiled on the target, so that is a hard floor, not a preference. None of
> these exist there:
>
> | Newer form | Needs | Use instead |
> |---|---|---|
> | `cmake -S . -B build` | 3.13 | `mkdir -p build && cd build && cmake ..` |
> | `cmake --build build -j N` | 3.12 | `make -j$(nproc)` from the build dir |
> | `cmake --install build` | 3.15 | `make install` |
> | `ctest --test-dir build` | 3.20 | `cd build && ctest` |
> | `target_link_directories()` | 3.13 | `link_directories()` before the target |

## Install

```bash
sudo make -C build install                      # binary + unit + doc  (cmake --install is 3.15+)
sudo mkdir -p /etc/jetson-ctrl
sudo cp config.example.json /etc/jetson-ctrl/config.json   # edit before first run!
sudo systemctl daemon-reload

# Bench-test in the foreground BEFORE handing it to systemd — Restart=always
# turns any startup failure into a 2-second restart loop that buries the cause.
sudo /usr/local/sbin/jetson-ctrl --config /etc/jetson-ctrl/config.json

sudo systemctl enable --now jetson-ctrl
journalctl -u jetson-ctrl -f
```

Installs to `CMAKE_INSTALL_PREFIX`, default `/usr/local` — so the binary lands
at `/usr/local/sbin/jetson-ctrl`, which is what `systemd/jetson-ctrl.service`
expects. Configure with `-DCMAKE_INSTALL_PREFIX=/usr` only if you also edit the
unit's `ExecStart` to match.

## Before first run — you MUST set the GPIO line offsets

The `line_offset` values in the config are placeholders. Map your physical 40-pin
header pins to their `gpiochip` lines and update `dht22.line_offset` and
`external_fan.line_offset`:

```bash
sudo gpioinfo            # list every line on every gpiochip
# or, if you know a line's label:
sudo gpiofind <LABEL>
```

The as-built pin map (DHT22 on pin 29 / line 149, fan on pin 31 / line 200) and
the wiring behind it are in
[docs/host-setup.md](docs/host-setup.md#gpio-allocation).

Also **bench-test the relay polarity** (`external_fan.active_high`) so the fan is
OFF at rest, and confirm NVIDIA's governor owns the built-in fan
(`systemctl status nvfancontrol`). The as-built rig uses an **active-low** relay
module, hence `"active_high": false` — do not assume yours matches:

```bash
sudo systemctl stop jetson-ctrl                # the daemon holds the line
sudo gpioset -m wait gpiochip0 200=1           # fan runs here => active_high: true
sudo gpioset -m wait gpiochip0 200=0           # fan runs here => active_high: false
```

## Config

Live config: `/etc/jetson-ctrl/config.json` (schema in `config.example.json`).
**Hot-reloaded** on file change — edit and save; a bad edit is rejected and the
last-known-good config keeps running.

**Every parameter explained, with defaults, validation rules and tuning advice:
[docs/config-reference.md](docs/config-reference.md).**

## Remote override

Set from the dashboard (`POST /api/v1/control`, `device_id: jetson_ctrl_01`):

```json
{ "external_fan_override": "force_on", "until": "2026-07-20T15:30:00Z" }
```

`force_on` runs the external fan early; it auto-expires at `until`. There is **no
force-off** — an override can only *add* cooling, and the autonomous thermal-zone
safety trigger always wins.

## Status

**Running on the target** (Jetson Nano, JetPack 4.x, 2026-07-21). Verified
end-to-end by hand:

- Builds on the Jetson's CMake 3.10.2.
- `dht22.cpp` bit-bang decodes cleanly — 23.8 °C / 66.2 % against a DHT22 on
  header pin 29, `enclosure_stale: false` (good checksums, not scraping by on
  retries).
- `thermal.cpp` resolves 2/2 zones; the control law decides and logs its reason.
- `fan.cpp` drives a real active-low relay on header pin 31; fan confirmed off
  at rest with the daemon running.
- Telemetry reaches the web-server and appears on the dashboard.

Not yet proven:

- **Unattended reboot.** It has been started by hand, not yet observed coming up
  clean on its own.
- **The remote override path** (`force_on` / expiry) — never exercised.
- **Sustained running.** No soak test, so nothing is known about DHT22 read
  reliability once the enclosure is hot, or about relay behaviour over many
  cycles. `dht22.cpp` logs nothing about retries or checksum failures, so a
  sensor going marginal would surface only as `enclosure_stale` flipping.
