# edge-ctrl

Host daemon that runs the SmartFarm enclosure's **external fan** as a second
cooling stage on single-board computers (**NVIDIA Jetson Nano** & **Raspberry Pi 3 Model B**), reports thermal telemetry to the Node web-server, and self-heals under systemd. See **[DESIGN.md](DESIGN.md)** for the full rationale and decision record.

> Not ESP firmware and not the Node server — this is a native C++17 Linux daemon
> that runs on the edge device itself.

## Layout

```
edge-ctrl/
├── src/ tests/ third_party/   # the C++17 daemon (this README)
├── CMakeLists.txt
├── systemd/edge-ctrl.service
├── config.example.json
├── python/                    # host provisioning: DS3231 RTC, DHT22 & Relay env setup
├── docs/hardware-spec.md      # Pin & GPIO spec for Jetson Nano & Raspberry Pi 3B
└── docs/host-setup.md         # wiring, GPIO map, RTC units, troubleshooting
```

The **[`python/`](python/)** tree is separate on purpose — it is one-shot host
setup that runs at boot and exits, not part of the control loop. Anything about
the machine underneath the daemon (I²C RTC, 40-pin header allocation, boot
units) lives in **[docs/host-setup.md](docs/host-setup.md)** and **[docs/hardware-spec.md](docs/hardware-spec.md)**.

## What it controls
- **Reads:** on-die thermal zones (`/sys/class/thermal`, the safety anchor) + a
  DHT22 enclosure-air sensor (secondary/telemetry).
- **Writes:** exactly one GPIO — the external fan relay/MOSFET.

## Build (on the Edge device)

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

## Install

```bash
sudo make -C build install                      # binary + unit + doc
sudo mkdir -p /etc/edge-ctrl
sudo cp config.example.json /etc/edge-ctrl/config.json   # edit before first run!
sudo systemctl daemon-reload

# Bench-test in the foreground BEFORE handing it to systemd
sudo /usr/local/sbin/edge-ctrl --config /etc/edge-ctrl/config.json

sudo systemctl enable --now edge-ctrl
journalctl -u edge-ctrl -f
```

Installs to `CMAKE_INSTALL_PREFIX`, default `/usr/local` — so the binary lands
at `/usr/local/sbin/edge-ctrl`, which is what `systemd/edge-ctrl.service`
expects.

## Config

Live config: `/etc/edge-ctrl/config.json` (schema in `config.example.json`).
**Hot-reloaded** on file change — edit and save.

Every parameter explained, with defaults, validation rules and tuning advice:
**[docs/config-reference.md](docs/config-reference.md).**

## Remote override

Set from the dashboard (`POST /api/v1/control`, `device_id: edge_ctrl_unit`):

```json
{ "external_fan_override": "force_on", "until": "2026-07-20T15:30:00Z" }
```
