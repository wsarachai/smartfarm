# AI Smart Farm System

## 1. Sensor Node 1 (Zone A)

**Hardware**

* ESP32
* DHT22
* Soil Moisture Sensor

**Role**
Environmental sensing for Zone A.

**Responsibilities**

* Temperature measurement
* Humidity measurement
* Soil moisture measurement
* Send data to Jetson Nano via HTTP

***

## 2. Sensor Node 2 (Zone B)

**Hardware**

* ESP32
* DHT22
* Soil Moisture Sensor

**Role**
Environmental sensing for Zone B.

**Responsibilities**

* Same as Node 1
* Monitor a different growing area

***

## 3. Pump Control Node

**Hardware**

* ESP32
* Relay
* Pump

**Role**
Actuator node.

**Responsibilities**

* Receive HTTP commands from Jetson Nano
* Turn pump ON/OFF
* Report status back to Jetson

***

## 4. ESP32-CAM IP Camera

### Role

Vision Sensor

Unlike DHT22 and Soil Moisture sensors which provide numeric data, the camera provides visual data.

### Responsibilities

* Live video streaming
* Capture snapshots
* Plant monitoring
* Remote field observation

### Example Data Flow

```text
Jetson Nano
      │
      ├── Request image
      ▼
ESP32-CAM

http://192.168.1.50/capture
```

or

```text
ESP32-CAM
      │
      ▼
MJPEG Stream

http://192.168.1.50:81/stream
```

***

# Why Not Use ESP32-CAM as Main Controller?

ESP32-CAM is good for:

✅ Streaming video

✅ Capturing images

✅ Basic image processing

But not ideal for:

❌ Database

❌ Dashboard server

❌ AI inference

❌ Multi-node management

Therefore:

```text
ESP32-CAM = Vision Sensor
Jetson Nano = Brain
```

***

# Updated Smart Farm Architecture

```text
                    ┌─────────────────┐
                    │ Sensor Node 1   │
                    │ DHT22           │
                    │ Soil Moisture   │
                    └───────┬─────────┘
                            │ HTTP
                            │
                            ▼

 ┌──────────────────────────────────────────┐
 │                                          │
 │               JETSON NANO                │
 │                                          │
 │  - FastAPI Server                        │
 │  - SQLite/PostgreSQL                     │
 │  - Dashboard Web Server                  │
 │  - Automation Logic                      │
 │  - AI Vision Processing                  │
 │                                          │
 └───────┬──────────────┬───────────────────┘
         │              │
         │HTTP          │HTTP Video/Image
         │              │
         ▼              ▼

 ┌─────────────┐   ┌──────────────┐
 │ Pump Node   │   │ ESP32-CAM    │
 │ Relay       │   │ IP Camera    │
 └─────────────┘   └──────────────┘

         ▲
         │
         │ HTTP
         │

 ┌─────────────┐
 │ Sensor Node2│
 │ DHT22       │
 │ Soil Moist. │
 └─────────────┘
```

***

# New Dashboard Design

The Jetson Nano should host the dashboard.

## Dashboard Home

```text
--------------------------------------------
SMART FARM CONTROL CENTER
--------------------------------------------

Zone A
Temp : 30.2 °C
Humidity : 72%
Soil : 45%

Zone B
Temp : 29.8 °C
Humidity : 70%
Soil : 35%

Pump Status
● ON

Camera Status
● Online
--------------------------------------------
```

***

## Live Camera Page

Embed ESP32-CAM stream inside dashboard.

```text
---------------------------------
        LIVE FARM CAMERA
---------------------------------

+------------------------+
|                        |
|    MJPEG STREAM        |
|                        |
+------------------------+

[Capture Image]
[Record Snapshot]
```

***

## Irrigation Control Page

```text
Pump Control

Mode:
( ) Auto
( ) Manual

Manual Control

[ Pump ON ]
[ Pump OFF ]
```

***

## AI Analytics Page

Jetson Nano can analyze images from ESP32-CAM.

Future functions:

### Plant Growth Monitoring

```text
Plant Height
Leaf Count
Canopy Coverage
Growth Rate
```

### Disease Detection

```text
Healthy : 95%

Possible disease:
Leaf Spot Detected
Confidence: 87%
```

### Water Stress Estimation

Combining:

```text
Camera Images
+
Temperature
+
Humidity
+
Soil Moisture
```

Jetson predicts:

```text
Water Stress Risk = Medium
```

***

# Recommended Control Philosophy

### Sensor Nodes

Only sense.

```text
Read Data
Send Data
```

***

### ESP32-CAM

Only capture images/video.

```text
Capture
Stream
```

***

### Pump Node

Only execute commands.

```text
ON
OFF
```

***

### Jetson Nano

Everything intelligent happens here.

```text
Collect Data
Store Data
Analyze Data
Run AI
Drive Dashboard
Control Pump
```

# Final Architecture Hierarchy

```text
Layer 1: Edge Devices
---------------------
Sensor Node 1
Sensor Node 2
ESP32-CAM
Pump Node

Layer 2: Smart Farm Brain
-------------------------
Jetson Nano

- API Server
- Database
- Dashboard
- Automation Engine
- AI Vision Engine

Layer 3: Users
--------------
Mobile
Tablet
Laptop
PC
```

For a professional deployment, I would designate the **Jetson Nano as the single "Smart Farm Control Center"**, while the ESP32-CAM acts as a **vision sensor**, providing images that the Jetson uses for AI-based plant monitoring and decision-making. This keeps the architecture clean, scalable, and easy to maintain.
