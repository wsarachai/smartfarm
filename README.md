# Smart Farm AgTech IoT & AI Edge Control Platform

A production-grade, multi-architecture Smart Agriculture (AgTech) edge computing platform for telemetry ingestion, field actuator control, automated irrigation scheduling, real-time video streaming, and edge AI insights (water stress analysis, green canopy coverage estimation, and crop disease classification).

Designed for resource-constrained Single-Board Computers (**NVIDIA Jetson Nano** and **Raspberry Pi 3 Model B**).

---

## 1. System Architecture

The platform follows a modular, microservice-based edge architecture:

```
                                  ┌────────────────────────────────────────────────────────┐
                                  │               Dashboard / Web Browser                  │
                                  └───────────────────────────┬────────────────────────────┘
                                                              │ HTTP (Port 3000)
                                                              ▼
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ Host System (NVIDIA Jetson Nano / Raspberry Pi 3B)                                                      │
│                                                                                                          │
│   ┌──────────────────────────────────────────────┐        POST /water-stress     ┌────────────────────┐  │
│   │ web-server (Docker Container: Port 3000)     │─────── POST /canopy ─────────►│ smartfarm-ai       │  │
│   │                                              │─────── POST /disease ────────►│ (Docker: Port 5000)│  │
│   │ - Express.js API + Static React RTK UI       │◄────── JSON Responses ────────│ - PyTorch / TFLite │  │
│   │ - Telemetry Ingestion & Control Endpoints    │                               └────────────────────┘  │
│   │ - ESP32-CAM MJPEG Video Streaming Proxy      │                                                       │
│   │ - Irrigation Pump Scheduler                  │                                                       │
│   └──────────────────────┬───────────────────────┘                                                       │
│                          │                                                                               │
│                          │ Local telemetry POST /api/v1/telemetry                                        │
│                          ▼                                                                               │
│   ┌──────────────────────────────────────────────┐                                                       │
│   │ edge-ctrl (Native C++17 Linux Daemon)        │                                                       │
│   │ - Enclosure External Cooling Fan Control     │                                                       │
│   │ - System Thermal Zone Monitoring             │                                                       │
│   │ - Host Python RTC Sync (DS3231 I²C)          │                                                       │
│   └──────────────────────────────────────────────┘                                                       │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
                                   ▲                            ▲
                                   │ Telemetry                  │ MJPEG Video Stream
                                   │                            │
 ┌─────────────────────────────────┴───┐                    ┌───┴─────────────────────────────────┐
 │ ESP32 Field Nodes (Firmware)        │                    │ ESP32-CAM Node                      │
 │ - sensor-zone: Soil & Temp Telemetry│                    │ - esp32cam: Live Video Feed         │
 │ - pump-zone: Relay & Water Valve    │                    └─────────────────────────────────────┘
 └─────────────────────────────────────┘
```

---

## 2. Directory Layout & Microservices

| Component | Architecture & Language | Role |
| :--- | :--- | :--- |
| **[`web-server/`](web-server)** | Node.js (Express) + React (Redux Toolkit) | Central Control Web Server, Static UI Host, REST API, Stream Proxy |
| **[`smartfarm-ai/`](smartfarm-ai)** | Python 3 (PyTorch / TFLite / OpenCV) | AI Decision Engine (Water Stress, Canopy Coverage, PlantVillage Disease Classifier) |
| **[`edge-ctrl/`](edge-ctrl)** | C++17 Native Daemon + Python Tooling | Host Enclosure Cooling Fan Daemon, Thermal Safety Governor, DS3231 RTC Timekeeper |
| **[`sensor-zone/`](sensor-zone)** | ESP-IDF (C/C++) | Field Sensor Node Firmware (Ingests Soil Moisture, Temp, Humidity) |
| **[`pump-zone/`](pump-zone)** | ESP-IDF (C/C++) | Irrigation Actuator Firmware (Controls Water Pumps & Solenoid Valves) |
| **[`esp32cam/`](esp32cam)** | Arduino (ESP32-CAM) | Live MJPEG Camera Streaming Firmware |

---

## 3. End-to-End Setup Guide

Follow this step-by-step installation guide to deploy the entire system on your host edge device.

---

### Step 1: Install Docker & Host Prerequisites

#### 1.1 Install Docker Engine & Compose
On your host device (Jetson Nano or Raspberry Pi), run:

```bash
# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Grant your user permission to run Docker without sudo
sudo usermod -aG docker $USER
newgrp docker

# Verify Docker installation
docker --version
docker compose version
```

#### 1.2 Target Hardware Prerequisites
- **For NVIDIA Jetson Nano (Ubuntu 18.04 LTS):**
  Ensure the NVIDIA Container Runtime is installed to allow `smartfarm-ai` access to GPU acceleration:
  ```bash
  sudo apt update && sudo apt install -y nvidia-container-runtime
  sudo systemctl restart docker
  ```
- **For Raspberry Pi 3 Model B (Raspberry Pi OS):**
  1. Enable the I²C bus for the DS3231 RTC module:
     ```bash
     sudo raspi-config nonint do_i2c 0
     ```
  2. Grant user group permissions:
     ```bash
     sudo usermod -aG gpio,i2c $USER
     ```

---

### Step 2: Set Up Hardware Control & Host Tooling (`edge-ctrl`)

`edge-ctrl` provisions hardware real-time clock synchronization, GPIO pin mappings, and runs the native cooling daemon.

#### 2.1 Install Host Provisioning & RTC Scripts
Run the automated installer script selecting your target board:

- **On NVIDIA Jetson Nano:**
  ```bash
  cd edge-ctrl/python
  sudo ./install.sh jetson
  ```
- **On Raspberry Pi 3 Model B:**
  ```bash
  cd edge-ctrl/python
  sudo ./install.sh rpi
  ```

#### 2.2 Verify GPIO & Relay Operation
Bench test the relay switch before starting systemd services:
```bash
sudo python3 relay.py --diag
sudo python3 relay.py on
sudo python3 relay.py off
```

#### 2.3 Build & Install the C++ Cooling Daemon
```bash
cd ~/workspace/smartfarm/edge-ctrl
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest

# Install binary and start systemd daemon
sudo make install
sudo systemctl daemon-reload
sudo systemctl enable --now edge-ctrl

# Check status
systemctl status edge-ctrl
```

---

### Step 3: Set Up the AI Decision Service (`smartfarm-ai`)

`smartfarm-ai` runs as an isolated microservice container on **Port 5000**.

#### 3.1 Download AI Model Weights (PlantVillage Disease Model)
Fetch the model checkpoint and class mappings:
```bash
cd ~/workspace/smartfarm/smartfarm-ai
./download_model.sh
```

#### 3.2 Launch AI Service Container

- **On NVIDIA Jetson Nano / x86 (`.pth` PyTorch Backend):**
  ```bash
  docker compose up -d --build
  # Convert model weights inside PyTorch container
  docker exec smartfarm-ai python3 /smartfarm-ai/convert_weights.py
  ```

- **On Raspberry Pi 3 Model B (`.tflite` TFLite Backend):**
  ```bash
  # Build and launch using the Raspberry Pi compose configuration
  docker compose -f docker-compose.rpi.yaml up -d --build
  ```

#### 3.3 Verify AI Microservice Health
```bash
curl http://localhost:5000/health
# Expected output: {"status":"ok"}
```

---

### Step 4: Set Up the Web Control Center (`web-server`)

`web-server` hosts the centralized Node.js/Express API, compiles static React dashboard assets, and runs on **Port 3000**.

#### 4.1 Configure Environment File
```bash
cd ~/workspace/smartfarm/web-server
cp .env.example .env
```
*(Verify that `AI_SERVICE_URL=http://smartfarm-ai:5000` is set).*

#### 4.2 Launch Web Server Container
```bash
docker compose up -d --build
```

#### 4.3 Alternative: Standalone Local Run (Development)
If running without Docker:
```bash
cd ~/workspace/smartfarm/web-server
npm install
npm run build
npm start
```

---

## 4. Verification & System Health Checklist

Once all services are up, verify full system integration:

1. **Dashboard Access:**
   Open browser at `http://<DEVICE_IP>:3000` to load the control dashboard.
2. **AI Microservice Status:**
   ```bash
   curl -s http://localhost:5000/health
   ```
3. **Container Health:**
   ```bash
   docker ps
   # Should list: smartfarm-web-server (Port 3000) & smartfarm-ai (Port 5000)
   ```
4. **Host Fan Daemon:**
   ```bash
   systemctl status edge-ctrl
   ```
5. **Hardware RTC Timekeeper:**
   ```bash
   systemctl status ds3231-sync.service
   ```

---

## 5. Firmware Build Instructions (Field Nodes)

Firmware for ESP32 and ESP8266 field devices can be built and flashed using **PlatformIO**:

```bash
# Flash Telemetry Sensor Zone (ESP-IDF)
cd sensor-zone && pio run -t upload

# Flash Irrigation Pump Controller (ESP-IDF)
cd pump-zone && pio run -t upload

# Flash ESP32-CAM Camera Node (Arduino)
cd esp32cam && pio run -t upload
```

---

## 6. Documentation References

- **[Architecture & Hardware Spec](edge-ctrl/docs/hardware-spec.md):** Side-by-side pinout matrix and Linux driver details.
- **[AI Decision Engine Documentation](smartfarm-ai/README.md):** API reference, HSV canopy calculation, PyTorch & TFLite converters.
- **[Web Server Developer Guide](web-server/DEV.md):** Express API routes, Redux Toolkit state slices, and pump scheduling algorithms.
- **[Edge Control Manual](edge-ctrl/README.md):** C++ cooling daemon configuration reference.
