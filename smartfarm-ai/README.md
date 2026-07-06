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
  CNN (default MobileNetV2). Torch is imported lazily on first call.
- `download_model.sh` + `convert_weights.py` + `models/` — fetch the weights +
  class names (gitignored), then normalize them into `models/disease.pth` +
  `models/model_config.json`. See "Set up the disease model" below.
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

Uses `Daksh159/plant-disease-mobilenetv2` by default (torchvision MobileNetV2,
38 PlantVillage classes, ImageNet preprocessing). On the Jetson:

```bash
# 1. download the checkpoint + class names into models/ (host-side)
cd smartfarm-ai && ./download_model.sh
# 2. normalize -> models/disease.pth + model_config.json (needs torch -> in the container)
docker exec smartfarm-ai python3 /smartfarm-ai/convert_weights.py
```

`convert_weights.py` loads the checkpoint into a torchvision MobileNetV2, re-saves
a clean state_dict in **legacy format** (readable by the Jetson's old torch), and
writes `model_config.json` with the **correct label order from `class_names.json`**.
Then hit **Analyze** on the dashboard — the model lazy-loads. (Already downloaded
the `.pth` manually into `models/`? Skip step 1; `convert_weights.py` fetches
`class_names.json` itself if missing.) Override the source with
`DISEASE_WEIGHTS_URL` / `DISEASE_CLASS_NAMES_URL`.

The web-server sends already-averaged fresh inputs + the thresholds; this service
holds no state. When it's unreachable the web-server degrades gracefully (shows
"AI offline").

## Run (on the Jetson)

Bring the base up first (it creates the shared `smartfarm-net`), then this service:

```bash
cd web-server     && docker compose -f docker-compose.yaml up -d      # web-server + network
cd ../smartfarm-ai && docker compose -f docker-compose.ai.yaml up -d  # AI service on :8000
```

The web-server reaches it at `http://smartfarm-ai:8000` over the shared network.

### Dev: JupyterLab

The service replaces JupyterLab as the default command. To develop models
interactively, launch Jupyter ad-hoc in the running container:

```bash
docker exec -it smartfarm-ai jupyter lab --ip=0.0.0.0 --allow-root
```

(or temporarily set `entrypoint`/`ports 8888` back in the compose file).

## Local test (no Jetson)

The decision service is pure stdlib, so it runs on any Python 3.6+:

```bash
cd smartfarm-ai && AI_SERVICE_PORT=8000 python3 ai_service.py
curl -s localhost:8000/health
curl -s -X POST localhost:8000/water-stress -H 'Content-Type: application/json' \
  -d '{"inputs":{"soilMoisture":22,"temperature":35,"humidity":40},"thresholds":{}}'
```
