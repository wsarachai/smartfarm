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
(`systemctl status nvfancontrol`).

## Config

Live config: `/etc/jetson-ctrl/config.json` (schema in `config.example.json`).
**Hot-reloaded** on file change — edit and save; a bad edit is rejected and the
last-known-good config keeps running. Full field reference in
[DESIGN.md](DESIGN.md#config-schema-etcjetson-ctrlconfigjson).

## Remote override

Set from the dashboard (`POST /api/v1/control`, `device_id: jetson_ctrl_01`):

```json
{ "external_fan_override": "force_on", "until": "2026-07-20T15:30:00Z" }
```

`force_on` runs the external fan early; it auto-expires at `until`. There is **no
force-off** — an override can only *add* cooling, and the autonomous thermal-zone
safety trigger always wins.

## Status

Scaffold. Hardware-touching paths (`dht22.cpp` bit-bang, `fan.cpp`/thermal GPIO)
are structured and commented but **not yet verified on real hardware** — build and
bench-test on the target Jetson before trusting it.
