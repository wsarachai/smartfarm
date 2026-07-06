# AI frame-pull contract

How the AI inference container gets camera images. The AI **pulls** frames from
the web-server over HTTP on a shared docker network — **no image files are
written to the SD card** (honors the project's no-SD-wear principle; frames stay
in the web-server's RAM ring exactly as before).

## Endpoints

### `GET /api/v1/camera/frame.jpg`
The latest JPEG the camera pushed. Response headers:

| Header | Meaning |
| --- | --- |
| `ETag` | `"<seq>"` — monotonic id of this frame |
| `Last-Modified` | when the frame was received (UTC) |
| `X-Frame-Seq` | the same `seq`, unquoted, for convenience |

Send the last `ETag` you saw back as **`If-None-Match`**:

- **`200 OK`** + JPEG body — a new frame (its `ETag`/`X-Frame-Seq` differ).
- **`304 Not Modified`** — same frame as last time; skip it (no re-inference).
- **`503 Service Unavailable`** — the camera hasn't pushed a frame yet.

This is standard HTTP conditional GET, so any client library gets dedup for free.

### `GET /api/v1/camera/status`
Cheap JSON poll (no pixels): `{ online, hasFrame, seq, ageMs, receivedAt, ... }`.
Use `seq` to detect a change without downloading the frame, or just rely on the
`If-None-Match` → `304` path above.

## Cadence

The camera pushes on a duty cycle (default one frame / 60 s — see
`CAMERA_SNAPSHOT_INTERVAL_MS`). You can't pull frames faster than the camera
sends them. Need a higher rate? Lower the interval in **Settings → Camera
Control** (it propagates to the camera each cycle). There is deliberately no
"capture now" trigger — the camera-v2 redesign removed continuous-demand load to
protect the camera.

## Networking

The base compose puts the web-server on a bridge network `smartfarm-net`. The AI
service joins it and reaches the server by name: **`http://web-server:3000`**.

Run **on the Jetson** (the AI service needs `runtime: nvidia`). Bring the base up
first — it creates the `smartfarm-net` network — then the AI overlay attaches to
it (it's declared `external`):

```bash
cd web-server      && docker compose -f docker-compose.yaml up -d      # web-server + smartfarm-net
cd ../smartfarm-ai && docker compose -f docker-compose.ai.yaml up -d   # AI service
```

Order matters: the base creates `smartfarm-net`, which the overlay attaches to as
an external network. Run the overlay first and you'll get *"network smartfarm-net
declared as external, but could not be found"* — start the web-server first.

The container (DLI image `nvcr.io/nvidia/dli/dli-nano-ai:v2.0.2-r32.7.1`) runs the
AI **decision service** (`smartfarm-ai/ai_service.py`) as its command — JupyterLab
is no longer the default (launch it ad-hoc for model dev; see `smartfarm-ai/README.md`).
The frame-pull reference artifacts live alongside it in `smartfarm-ai/`:

- `smartfarm_inference.ipynb` — grab one frame + a continuous ETag-dedup loop.
- `frame_poller.py` — the same loop as a headless script.

Both read `WEB_SERVER_URL` (default `http://web-server:3000`) and call `infer()` —
replace that with your model.
