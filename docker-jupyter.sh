#!/usr/bin/env bash
# Launch JupyterLab for AI model development in the NVIDIA DLI image.
#
# Separate from the smartfarm-ai DECISION service container (which runs
# ai_service.py) — this uses the image's default JupyterLab command, on the
# shared smartfarm-net network, with the smartfarm-ai/ code mounted under
# data/smartfarm. Open http://<jetson>:8888 (password: dlinano). Ctrl-C to stop
# (--rm cleans up).
#
# Run ON THE JETSON, after the base compose is up (it creates smartfarm-net):
#   cd web-server && docker compose -f docker-compose.yaml up -d
#   ./docker-jupyter.sh
set -euo pipefail

# Repo root = this script's directory, so it works from any cwd.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

docker run --rm -it --runtime nvidia \
  --name smartfarm-ai-jupyter \
  --network smartfarm-net \
  -p 8888:8888 \
  -v "$ROOT/smartfarm-ai:/nvdli-nano/data/smartfarm" \
  nvcr.io/nvidia/dli/dli-nano-ai:v2.0.2-r32.7.1
