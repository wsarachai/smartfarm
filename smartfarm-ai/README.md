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
- `frame_poller.py` / `smartfarm_inference.ipynb` — dev artifacts for the camera
  frame-pull path (`../web-server/docs/ai-frame-pull.md`); used interactively.

## API (called by the web-server)

- `GET  /health` → `{"status":"ok"}`
- `POST /water-stress` → body `{ inputs:{soilMoisture,temperature,humidity},
  thresholds:{…} }` → `{ band, risk, factors }`.

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
