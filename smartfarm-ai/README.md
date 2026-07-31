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
- `download_model.sh` + `convert_weights.py` (torch hosts) + `convert_to_tflite.py`
  (Raspberry Pi) + `models/` — fetch the weights + class names (gitignored), then
  normalize them into `models/disease.{pth,tflite}` + `models/model_config.json`.
  See "Set up the disease model" below.
- `verify_class_names.py` + `plantvillage_classes.json` — the **upstream label
  file is broken** (37 entries for a 38-class checkpoint: `Tomato___healthy`
  missing, `Tomato___Tomato_mosaic_virus` mangled). This validates and repairs it
  against the committed canonical list. Run automatically by `download_model.sh`
  and by both converters, so a re-download can't silently reintroduce the fault.
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
38 PlantVillage classes, ImageNet preprocessing).

**Pick the path that matches your host** — this is a hardware constraint, not a
preference. PyTorch publishes **no 32-bit ARM wheels** (zero `armv7l` files on
PyPI, any version), so on a Raspberry Pi running a 32-bit OS `import torch` can
never succeed. `tflite-runtime` does ship `armv7l` wheels, so the Pi runs the
same network through the TFLite interpreter. Check yours with `uname -m`.

### Jetson / x86 / aarch64 (`.pth`, torch backend)

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

### Raspberry Pi, 32-bit (`.tflite`, tflite backend)

Convert **on a dev box** — the toolchain (torch + tensorflow + onnx2tf, ~2 GB)
is exactly what the Pi can't install, and it's only needed once. The Pi consumes
just the ~9 MB output.

```bash
# on the dev machine, in smartfarm-ai/
./download_model.sh                     # or copy models/mobilenetv2_plant.pth + class_names.json across
python3 -m venv .venv && . .venv/bin/activate
pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu
pip install onnx onnx2tf tensorflow-cpu onnx_graphsurgeon sng4onnx simple_onnx_processing_tools onnxruntime psutil
python3 convert_to_tflite.py            # -> models/disease.tflite + model_config.json

# then, on the Pi
scp models/disease.tflite models/model_config.json pi@<pi>:~/…/smartfarm-ai/models/
docker compose -f docker-compose.rpi.yaml up -d --build   # --build picks up tflite-runtime
```

The pipeline is torch → ONNX → TFLite; `onnx2tf` does the NCHW→NHWC transpose
TFLite needs. `convert_to_tflite.py` finishes by running **both** models on
`models/leaf.jpg` and comparing logits — if it prints a disagreement warning,
don't ship that model. Expect a few seconds per inference on a Pi 3 B.

### A note on the labels

The upstream `class_names.json` is **broken**: 37 entries for a 38-class
checkpoint, with index 37 (`Tomato___healthy`) absent and index 36 mangled to
`Tomato___Tomato_mosaic_Normally`. Both converters used to pad the gap with a
synthetic `class_37`, which matters more than it looks — the web-server picks
its headline by matching `/healthy/` against the label, so a healthy tomato was
reported as "Possible: class_37".

`verify_class_names.py` now repairs it against `plantvillage_classes.json`
(the canonical 38 in ImageFolder order) and is invoked by `download_model.sh`
and both converters. It only rewrites files it recognises as PlantVillage
(≥90% positional match); a genuinely different model's labels are left alone,
and a label file with holes is rejected rather than guessed at. Re-running it is
a no-op, and the first original is preserved as `class_names.json.orig`.

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
