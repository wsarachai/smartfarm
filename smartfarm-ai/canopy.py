"""Canopy coverage — the AI decision for feature 2 (classical, no model).

Stateless: given a JPEG (bytes) + HSV thresholds, return the % of pixels that
fall in the green (canopy) range, plus a small mask-preview PNG (the downscaled
frame with detected canopy pixels highlighted) for tuning. PIL + numpy only
(no cv2). Runs on Python 3.6.

Thresholds come from the web-server (which owns settings) as intuitive units:
hue in DEGREES (0-360), saturation/value as PERCENT (0-100). PIL's HSV channels
are 0-255, so we convert.
"""
from io import BytesIO

import numpy as np
from PIL import Image

# Analyze at most this many pixels wide/tall — canopy % is scale-invariant, so a
# downscale keeps it fast on the Jetson without changing the result meaningfully.
MAX_DIM = 320

DEFAULT_PARAMS = {
    "hueMinDeg": 60,   # yellow-green
    "hueMaxDeg": 170,  # cyan-green
    "satMinPct": 20,   # exclude washed-out / grey pixels (soil, sky, shadow)
    "valMinPct": 15,   # exclude near-black shadow
}


def _num(v, fallback):
    try:
        return float(v)
    except (TypeError, ValueError):
        return fallback


def analyze(jpeg_bytes, params=None):
    p = dict(DEFAULT_PARAMS)
    for k in DEFAULT_PARAMS:
        if params and k in params:
            p[k] = _num(params[k], DEFAULT_PARAMS[k])

    img = Image.open(BytesIO(jpeg_bytes)).convert("RGB")
    img.thumbnail((MAX_DIM, MAX_DIM))  # in-place downscale, keeps aspect

    hsv = np.asarray(img.convert("HSV"))  # H,S,V each 0-255
    h = hsv[..., 0].astype(np.float32) * (360.0 / 255.0)  # -> degrees
    s = hsv[..., 1].astype(np.float32) * (100.0 / 255.0)  # -> percent
    v = hsv[..., 2].astype(np.float32) * (100.0 / 255.0)  # -> percent

    mask = (
        (h >= p["hueMinDeg"]) & (h <= p["hueMaxDeg"]) & (s >= p["satMinPct"]) & (v >= p["valMinPct"])
    )
    canopy_pct = round(float(mask.mean()) * 100.0, 1)

    factors = [
        "%.1f%% of pixels in the green range (hue %d-%d deg, S>=%d%%, V>=%d%%)."
        % (canopy_pct, int(p["hueMinDeg"]), int(p["hueMaxDeg"]), int(p["satMinPct"]), int(p["valMinPct"]))
    ]

    return {
        "canopyPercent": canopy_pct,
        "factors": factors,
        "width": img.width,
        "height": img.height,
        "maskPng": _preview_png(img, mask),
    }


def _preview_png(img, mask):
    """Desaturate the frame and paint detected canopy pixels bright green, so you
    can SEE what's counted while tuning thresholds. Returns a base64 PNG (no
    data: prefix)."""
    import base64

    rgb = np.asarray(img).astype(np.float32)
    gray = rgb.mean(axis=2, keepdims=True)
    out = np.repeat(gray * 0.55, 3, axis=2)  # dim grayscale background
    out[mask] = np.array([40, 230, 90], dtype=np.float32)  # highlight canopy
    preview = Image.fromarray(out.clip(0, 255).astype(np.uint8))  # mode inferred (RGB)

    buf = BytesIO()
    preview.save(buf, format="PNG")
    return base64.b64encode(buf.getvalue()).decode("ascii")
