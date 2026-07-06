"""Disease detection — the AI decision for feature 3 (a real CNN classifier).

A pretrained PlantVillage classifier (default MobileNetV2) that labels a leaf
image healthy vs a disease. Everything is config-driven so any compatible
checkpoint works: drop weights + a model_config.json into models/ (see the
download script). Torch is imported LAZILY on the first request, so the service
starts fast and the water-stress/canopy endpoints don't pay the torch cost
(and the container still runs if no model is present).

Stateless from the caller's view: JPEG bytes in -> top-k {label, confidence}.
The web-server applies the confidence threshold + healthy/disease headline.
"""
import json
import os
from io import BytesIO

MODELS_DIR = os.environ.get("DISEASE_MODELS_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), "models"))
CONFIG_PATH = os.path.join(MODELS_DIR, "model_config.json")
DEFAULT_TOPK = int(os.environ.get("DISEASE_TOPK", "3"))

# Lazily-initialized singletons (loaded once, on first inference).
_state = {"loaded": False, "error": None, "model": None, "labels": None, "cfg": None, "tf": None}


def _load_config():
    if not os.path.exists(CONFIG_PATH):
        return None, "no model_config.json in models/ (run download_model.sh or drop weights)"
    with open(CONFIG_PATH) as f:
        cfg = json.load(f)
    weights = os.path.join(MODELS_DIR, cfg.get("weights", "disease.pth"))
    if not os.path.exists(weights):
        return None, "weights file missing: %s" % os.path.basename(weights)
    cfg["_weights_path"] = weights
    return cfg, None


def _build(cfg):
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


def _ensure_loaded():
    if _state["loaded"] or _state["error"]:
        return
    cfg, err = _load_config()
    if err:
        _state["error"] = err
        return
    try:
        model, tf = _build(cfg)
        _state.update(loaded=True, model=model, labels=cfg["labels"], cfg=cfg, tf=tf, error=None)
    except Exception as exc:  # noqa: BLE001
        _state["error"] = "model load failed: %s" % exc


def classify(jpeg_bytes, topk=DEFAULT_TOPK):
    """Return { modelLoaded, topK:[{label,confidence}], ... } or a not-loaded note."""
    _ensure_loaded()
    if not _state["loaded"]:
        return {"modelLoaded": False, "error": _state["error"], "topK": []}

    import torch
    from PIL import Image

    img = Image.open(BytesIO(jpeg_bytes)).convert("RGB")
    x = _state["tf"](img).unsqueeze(0)
    with torch.no_grad():
        probs = torch.softmax(_state["model"](x)[0], dim=0)
    k = min(topk, probs.numel())
    vals, idx = torch.topk(probs, k)
    labels = _state["labels"]
    top = [
        {"label": labels[int(i)], "confidence": round(float(v) * 100.0, 1)}
        for v, i in zip(vals.tolist(), idx.tolist())
    ]
    return {"modelLoaded": True, "topK": top, "arch": _state["cfg"].get("arch")}
