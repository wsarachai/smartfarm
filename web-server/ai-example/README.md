# ai-example — SmartFarm AI inference starter

Reference artifacts for pulling camera frames into the NVIDIA DLI Jetson image
(`nvcr.io/nvidia/dli/dli-nano-ai:v2.0.2-r32.7.1`) and running inference on them.
Frames are pulled over HTTP (ETag/304 dedup); nothing is written to the SD card.

Full contract: [`../docs/ai-frame-pull.md`](../docs/ai-frame-pull.md).

## Run (on the Jetson)

Bring the base up first (creates the `smartfarm-net` network), then the overlay:

```bash
cd web-server
docker compose -f docker-compose.yaml up -d       # web-server + network
docker compose -f docker-compose.ai.yaml up -d    # AI container
```

Open JupyterLab at <http://JETSON:8888> (password `dlinano`). These files appear
under `data/smartfarm/`:

- **`smartfarm_inference.ipynb`** — interactive: fetch one frame, then a
  continuous inference loop. Start here.
- **`frame_poller.py`** — the same loop, headless. In a terminal:
  `python3 data/smartfarm/frame_poller.py`.

## Wire in your model

Both call a `infer(img, seq)` placeholder that just prints. Replace it with your
model (torchvision, jetcam, a TensorRT engine, …). `img` is a `PIL.Image` (RGB);
`seq` is the frame's monotonic id.

Config via env (set in `docker-compose.ai.yaml`):

- `WEB_SERVER_URL` — default `http://web-server:3000`
- `POLL_SECONDS` — poll cadence (default `5`; the camera pushes ~1/min, so most
  polls return `304`)
