#!/usr/bin/env python3
"""Reference frame poller for the SmartFarm AI inference container.

Pulls the latest camera frame from the web-server and runs (placeholder)
inference ONLY on new frames, using HTTP ETag / 304 Not Modified for dedup —
so a slow ~1/min camera doesn't get re-inferred every poll. No files are
written to the SD card; frames are pulled over the shared docker network.

Runs inside nvcr.io/nvidia/dli/dli-nano-ai:v2.0.2-r32.7.1 (Python 3.6;
`requests` and `PIL` are already installed). Replace infer() with a real model.

See ../docs/ai-frame-pull.md for the full contract.
"""
import io
import os
import time

import requests
from PIL import Image

BASE = os.environ.get("WEB_SERVER_URL", "http://web-server:3000")
FRAME_URL = BASE + "/api/v1/camera/frame.jpg"
POLL_SECONDS = float(os.environ.get("POLL_SECONDS", "5"))


def infer(img, seq):
    """Placeholder inference. Swap in torchvision / jetcam / your model here."""
    print("inferred frame seq=%s size=%s" % (seq, img.size), flush=True)


def main():
    etag = None  # last frame identity we've seen; None until the first frame
    print("polling %s every %ss" % (FRAME_URL, POLL_SECONDS), flush=True)
    while True:
        headers = {"If-None-Match": etag} if etag else {}
        try:
            resp = requests.get(FRAME_URL, headers=headers, timeout=10)
        except requests.RequestException as exc:
            print("request failed: %s" % exc, flush=True)
            time.sleep(POLL_SECONDS)
            continue

        if resp.status_code == 304:
            pass  # unchanged frame — nothing new to infer
        elif resp.status_code == 200:
            etag = resp.headers.get("ETag", etag)
            seq = resp.headers.get("X-Frame-Seq")
            img = Image.open(io.BytesIO(resp.content)).convert("RGB")
            infer(img, seq)
        elif resp.status_code == 503:
            print("no frame yet (camera hasn't pushed)", flush=True)
        else:
            print("unexpected status %s" % resp.status_code, flush=True)

        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    main()
