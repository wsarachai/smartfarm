"""Disease detection — the AI decision for feature 3 (a real CNN classifier).

A pretrained PlantVillage classifier (default MobileNetV2) that labels a leaf
image healthy vs a disease. Everything is config-driven so any compatible
checkpoint works: drop weights + a model_config.json into models/ (see the
download script).

TWO BACKENDS, picked by model_config.json's "runtime" (or inferred from the
weights extension), because the two target hosts can't run the same one:
  - "torch"  (.pth)    Jetson / x86 — torchvision MobileNetV2.
  - "tflite" (.tflite) Raspberry Pi — PyTorch publishes NO 32-bit ARM (armv7l)
                       wheels, so a Pi 3 B literally cannot import torch;
                       tflite-runtime does ship armv7l wheels. Convert the
                       checkpoint off-Pi with convert_to_tflite.py.

The heavy runtime is imported LAZILY on the first request, so the service starts
fast, the water-stress/canopy endpoints don't pay for it, and the container still
runs with no model present.

Stateless from the caller's view: JPEG bytes in -> top-k {label, confidence}.
The web-server applies the confidence threshold + healthy/disease headline.
"""
import json
import os
import threading
import time
from io import BytesIO

MODELS_DIR = os.environ.get("DISEASE_MODELS_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), "models"))
CONFIG_PATH = os.path.join(MODELS_DIR, "model_config.json")
DEFAULT_TOPK = int(os.environ.get("DISEASE_TOPK", "3"))

# A failed load used to latch forever, so dropping a model into models/ did
# nothing until the container was restarted (this cost real debugging time).
# Now a failure is remembered only this long, then retried on the next request.
RETRY_AFTER_S = float(os.environ.get("DISEASE_RETRY_AFTER_S", "30"))

# Lazily-initialized singletons (loaded once, on first inference).
_state = {"loaded": False, "error": None, "failed_at": 0.0, "model": None, "labels": None, "cfg": None, "tf": None}

# The service is threaded (ai_service.py), but neither backend is thread-safe:
# a TFLite Interpreter holds one set of input/output tensors, so two concurrent
# invokes would interleave and return each other's results. Serialize both the
# lazy load and the inference. Cheap endpoints (/health, /canopy) stay parallel.
_lock = threading.Lock()


def _load_config():
    if not os.path.exists(CONFIG_PATH):
        return None, "no model_config.json in models/ (run download_model.sh or drop weights)"
    with open(CONFIG_PATH) as f:
        cfg = json.load(f)
    weights = os.path.join(MODELS_DIR, cfg.get("weights", "disease.pth"))
    if not os.path.exists(weights):
        return None, "weights file missing: %s" % os.path.basename(weights)
    cfg["_weights_path"] = weights
    cfg.setdefault("runtime", "tflite" if weights.endswith(".tflite") else "torch")
    return cfg, None


# --- torch backend (Jetson / x86) ------------------------------------------

def _build_torch(cfg):
    """Import torch lazily + build the configured architecture with its weights.
    Returns (model, transform) or raises."""
    import torch
    import torchvision.transforms as T
    from torchvision import models as tvm

    arch = cfg.get("arch", "mobilenet_v2")
    num_classes = int(cfg["numClasses"])
    builders = {
        "mobilenet_v2": lambda: _swap_head_mobilenet(tvm.mobilenet_v2(pretrained=False), num_classes),
        "resnet18": lambda: _swap_head_resnet(tvm.resnet18(pretrained=False), num_classes),
        "resnet50": lambda: _swap_head_resnet(tvm.resnet50(pretrained=False), num_classes),
    }
    if arch not in builders:
        raise ValueError("unsupported arch: %s" % arch)
    model = builders[arch]()
    state = torch.load(cfg["_weights_path"], map_location="cpu")
    state = state.get("state_dict", state) if isinstance(state, dict) else state
    model.load_state_dict(state)
    model.eval()

    size = int(cfg.get("inputSize", 224))
    mean = cfg.get("mean", [0.485, 0.456, 0.406])
    std = cfg.get("std", [0.229, 0.224, 0.225])
    tf = T.Compose([T.Resize((size, size)), T.ToTensor(), T.Normalize(mean=mean, std=std)])
    return model, tf


def _swap_head_mobilenet(m, n):
    import torch.nn as nn

    m.classifier[1] = nn.Linear(m.classifier[1].in_features, n)
    return m


def _swap_head_resnet(m, n):
    import torch.nn as nn

    m.fc = nn.Linear(m.fc.in_features, n)
    return m


def _infer_torch(cfg, jpeg_bytes, topk):
    import torch
    from PIL import Image

    img = Image.open(BytesIO(jpeg_bytes)).convert("RGB")
    x = _state["tf"](img).unsqueeze(0)
    with torch.no_grad():
        probs = torch.softmax(_state["model"](x)[0], dim=0)
    k = min(topk, probs.numel())
    vals, idx = torch.topk(probs, k)
    return list(zip(vals.tolist(), idx.tolist()))


# --- tflite backend (Raspberry Pi armv7l) -----------------------------------

# The TFLite interpreter has moved twice. Try every home, newest-Pi-first:
#   tflite_runtime  the standalone wheel — the ONLY one with armv7l builds, so
#                   this is what the Pi container actually uses.
#   ai_edge_litert  where Google moved it; TF >=2.20 no longer exposes tf.lite.
#   tensorflow.lite older TensorFlow.
_INTERPRETER_HOMES = (
    ("tflite_runtime.interpreter", "Interpreter"),
    ("ai_edge_litert.interpreter", "Interpreter"),
    ("tensorflow.lite", "Interpreter"),
)


def _find_interpreter():
    import importlib

    for mod, attr in _INTERPRETER_HOMES:
        try:
            found = getattr(importlib.import_module(mod), attr, None)
        except ImportError:
            continue
        if found is not None:
            return found
    raise ImportError(
        "no TFLite interpreter available — install tflite-runtime (tried %s)"
        % ", ".join(m for m, _ in _INTERPRETER_HOMES)
    )


def _build_tflite(cfg):
    """Load the .tflite model. Prefers the tiny tflite_runtime wheel (the only
    thing installable on armv7l); falls back to full TensorFlow for dev boxes.
    Returns (interpreter, None) — preprocessing is numpy here, not a torchvision
    transform."""
    Interpreter = _find_interpreter()
    interp = Interpreter(model_path=cfg["_weights_path"], num_threads=int(os.environ.get("DISEASE_THREADS", "2")))
    interp.allocate_tensors()
    return interp, None


def _infer_tflite(cfg, jpeg_bytes, topk):
    import numpy as np
    from PIL import Image

    interp = _state["model"]
    inp = interp.get_input_details()[0]
    out = interp.get_output_details()[0]

    size = int(cfg.get("inputSize", 224))
    # Squashing resize (not aspect-preserving) to match torchvision's
    # T.Resize((size, size)) — the preprocessing the weights were trained with.
    img = Image.open(BytesIO(jpeg_bytes)).convert("RGB").resize((size, size), Image.BILINEAR)
    x = np.asarray(img, dtype=np.float32) / 255.0
    mean = np.array(cfg.get("mean", [0.485, 0.456, 0.406]), dtype=np.float32)
    std = np.array(cfg.get("std", [0.229, 0.224, 0.225]), dtype=np.float32)
    x = (x - mean) / std
    x = np.expand_dims(x, 0)  # NHWC — tflite's layout (the converter transposes from NCHW)

    if inp["dtype"] in (np.uint8, np.int8):  # quantized model — fold the input scale in
        scale, zero = inp["quantization"]
        if scale:
            x = x / scale + zero
        x = x.astype(inp["dtype"])
    else:
        x = x.astype(np.float32)

    interp.set_tensor(inp["index"], x)
    interp.invoke()
    logits = interp.get_tensor(out["index"])[0].astype(np.float32)

    if out["dtype"] in (np.uint8, np.int8):  # dequantize back to real numbers
        scale, zero = out["quantization"]
        if scale:
            logits = (logits - zero) * scale

    if cfg.get("applySoftmax", True):
        e = np.exp(logits - logits.max())
        probs = e / e.sum()
    else:
        probs = logits

    k = min(topk, probs.shape[0])
    idx = np.argsort(probs)[::-1][:k]
    return [(float(probs[i]), int(i)) for i in idx]


# --- shared -----------------------------------------------------------------

_BACKENDS = {
    "torch": (_build_torch, _infer_torch),
    "tflite": (_build_tflite, _infer_tflite),
}


def _ensure_loaded():
    if _state["loaded"]:
        return
    if _state["error"] and (time.time() - _state["failed_at"]) < RETRY_AFTER_S:
        return  # still in the cooldown — don't re-pay a failing import every request

    cfg, err = _load_config()
    if err:
        _state.update(error=err, failed_at=time.time())
        return
    runtime = cfg["runtime"]
    if runtime not in _BACKENDS:
        _state.update(error="unsupported runtime: %s" % runtime, failed_at=time.time())
        return
    try:
        model, tf = _BACKENDS[runtime][0](cfg)
        _state.update(loaded=True, model=model, labels=cfg["labels"], cfg=cfg, tf=tf, error=None)
    except ImportError as exc:  # the common Pi case — say what to do about it
        hint = " (this host has no torch — convert with convert_to_tflite.py)" if runtime == "torch" else ""
        _state.update(error="model load failed: %s%s" % (exc, hint), failed_at=time.time())
    except Exception as exc:  # noqa: BLE001
        _state.update(error="model load failed: %s" % exc, failed_at=time.time())


def classify(jpeg_bytes, topk=DEFAULT_TOPK):
    """Return { modelLoaded, topK:[{label,confidence}], ... } or a not-loaded note."""
    with _lock:
        _ensure_loaded()
        if not _state["loaded"]:
            return {"modelLoaded": False, "error": _state["error"], "topK": []}

        cfg = _state["cfg"]
        scored = _BACKENDS[cfg["runtime"]][1](cfg, jpeg_bytes, topk)
        labels = _state["labels"]
    top = [{"label": labels[i], "confidence": round(v * 100.0, 1)} for v, i in scored]
    return {"modelLoaded": True, "topK": top, "arch": cfg.get("arch"), "runtime": cfg["runtime"]}
