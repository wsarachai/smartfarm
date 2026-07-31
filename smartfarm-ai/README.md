# smartfarm-ai — AI decision service

The AI "brain" container for the Smart Farm. All AI decision logic lives here,
separate from the web-server (which stays AI-agnostic: it owns data, settings,
history, and the UI, and CALLS this service to decide).

Built on the NVIDIA DLI Jetson image (`nvcr.io/nvidia/dli/dli-nano-ai:v2.0.2-r32.7.1`)
so future GPU/torch vision endpoints (canopy coverage, disease detection — see
`../web-server/docs/ai-features-roadmap.md`) have a home. **Jetson-only** (needs
`runtime: nvidia`).

## Files

- `ai_service.py` — a tiny **stdlib** HTTP server (no pip deps, Python 3.6). Run
  as the container command.
- `water_stress.py` — the stateless water-stress **decision** (bands + evaporative
  adjust + factors), called by the service.
- `canopy.py` — canopy-coverage **decision** (feature 2): % green pixels via HSV
  thresholding (PIL + numpy) + a mask-preview PNG.
- `disease.py` — disease **decision** (feature 3): a config-driven PlantVillage
  CNN (default MobileNetV2), with **two backends** (torch / tflite) chosen by
  `model_config.json`. The runtime is imported lazily on first call.
- `download_model.sh` + `models/` — fetches pre-converted weights, configs, and label
  files directly from `wsarachai/plant-disease-mobilenetv2` on Hugging Face into
  `models/disease.{pth,tflite}` + `models/model_config.json`. See "Set up the disease model" below.
- `verify_class_names.py` + `plantvillage_classes.json` — validates and verifies label
  integrity against the canonical 38-class PlantVillage list. Run automatically by
  `download_model.sh`.
- `frame_poller.py` / `smartfarm_inference.ipynb` — dev artifacts for the camera
  frame-pull path (`../web-server/docs/ai-frame-pull.md`); used interactively.

## API (called by the web-server)

- `GET  /health` → `{"status":"ok"}`
- `POST /water-stress` → body `{ inputs:{soilMoisture,temperature,humidity},
  thresholds:{…} }` → `{ band, risk, factors }`.
- `POST /canopy?hueMinDeg=&hueMaxDeg=&satMinPct=&valMinPct=` → **raw JPEG body** →
  `{ canopyPercent, factors, maskPng (base64 PNG), width, height }`.
- `POST /disease` → **raw JPEG body** → `{ modelLoaded, topK:[{label,confidence}] }`.
  Needs a model (see below) — else `modelLoaded:false`.

## Set up the disease model

Downloads pre-converted model files directly from [wsarachai/plant-disease-mobilenetv2](https://huggingface.co/buckets/wsarachai/plant-disease-mobilenetv2) on Hugging Face (MobileNetV2, 38 PlantVillage classes, ImageNet preprocessing).

### 1. Download Model Assets (Host)

Run `download_model.sh` on your host device to fetch weights into the local `./models` directory (which is mounted into the container via volume):

```bash
cd smartfarm-ai && ./download_model.sh
```

This script fetches pre-packaged model files directly into `models/`:
- `disease.pth` — PyTorch model checkpoint (Jetson / x86)
- `disease.tflite` — TensorFlow Lite model (Raspberry Pi / 32-bit ARM)
- `model_config.json` — Architecture and backend runtime configuration
- `class_names.json` — Corrected 38-class PlantVillage labels

---

### 2. Launch Docker Container (Hardware-Specific)

Running `./download_model.sh` alone downloads files to the host, but `smartfarm-ai` runs inside a Docker container so the `web-server` container can reach it at `http://smartfarm-ai:8000` via the shared `smartfarm-net` network.

You **must build and start the Docker container** for your target board:

- **NVIDIA Jetson Nano / x86 (`.pth` PyTorch Backend):**
  ```bash
  cd smartfarm-ai && docker compose up -d --build
  ```

- **Raspberry Pi 3B / 32-bit ARM (`.tflite` TFLite Backend):**
  ```bash
  cd smartfarm-ai && docker compose -f docker-compose.rpi.yaml up -d --build
  ```

Once running, hit **Analyze** on the dashboard — the model lazy-loads automatically on first request.

---

### 3. Verification & System Health

Verify that the AI service container is running and healthy:

```bash
curl http://localhost:8000/health
# Expected response: {"status":"ok"}
```

### Note on Upstream Label Corrections

The original upstream labels were fixed against `plantvillage_classes.json` (canonical 38 classes in ImageFolder order). `verify_class_names.py` automatically validates label integrity upon download.

The web-server sends already-averaged fresh inputs + the thresholds; this service
holds no state. When it's unreachable the web-server degrades gracefully (shows
"AI offline").

## Dev: JupyterLab (Jetson Ad-hoc)

The service replaces JupyterLab as the default command. To develop models
interactively on Jetson, launch Jupyter ad-hoc in the running container:

```bash
docker exec -it smartfarm-ai jupyter lab --ip=0.0.0.0 --allow-root
```

## Local test (Standalone Python without Docker)

The decision service is pure stdlib, so it runs on any Python 3.6+:

```bash
cd smartfarm-ai && AI_SERVICE_PORT=8000 python3 ai_service.py
curl -s localhost:8000/health
curl -s -X POST localhost:8000/water-stress -H 'Content-Type: application/json' \
  -d '{"inputs":{"soilMoisture":22,"temperature":35,"humidity":40},"thresholds":{}}'
```
