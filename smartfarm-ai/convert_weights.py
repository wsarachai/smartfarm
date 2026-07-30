#!/usr/bin/env python3
"""Normalize a downloaded PlantVillage MobileNetV2 checkpoint for /disease.

Reads models/mobilenetv2_plant.pth (the raw download), loads it into a
torchvision MobileNetV2 (Dropout(0.2) + Linear head - torchvision's default),
and writes:
  - models/disease.pth        a clean state_dict, re-saved in LEGACY format so
                              the Jetson's old torch (<1.6) can read it too, and
  - models/model_config.json  arch + labels (from class_names.json) + ImageNet
                              preprocessing, so disease.py loads it with no code
                              change.

Run where torch/torchvision exist - i.e. INSIDE the smartfarm-ai container:
    docker exec smartfarm-ai python3 /smartfarm-ai/convert_weights.py

Env overrides: DISEASE_SRC_WEIGHTS, DISEASE_CLASS_NAMES, DISEASE_CLASS_NAMES_URL.
Defaults target the Daksh159/plant-disease-mobilenetv2 model (38 classes).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODELS = os.path.join(HERE, "models")
SRC = os.environ.get("DISEASE_SRC_WEIGHTS", os.path.join(MODELS, "mobilenetv2_plant.pth"))
OUT = os.path.join(MODELS, "disease.pth")
CONFIG = os.path.join(MODELS, "model_config.json")
CLASS_NAMES = os.environ.get("DISEASE_CLASS_NAMES", os.path.join(MODELS, "class_names.json"))
CLASS_NAMES_URL = os.environ.get(
    "DISEASE_CLASS_NAMES_URL",
    "https://huggingface.co/Daksh159/plant-disease-mobilenetv2/resolve/main/class_names.json",
)
MEAN = [0.485, 0.456, 0.406]
STD = [0.229, 0.224, 0.225]


def load_labels():
    """Ordered list of class names from class_names.json (downloaded if absent)."""
    if not os.path.exists(CLASS_NAMES):
        try:
            from urllib.request import urlopen

            print("Fetching class names: %s" % CLASS_NAMES_URL)
            data = urlopen(CLASS_NAMES_URL, timeout=30).read()
            with open(CLASS_NAMES, "wb") as f:
                f.write(data)
        except Exception as exc:
            sys.exit("ERROR: no %s and download failed (%s). Provide it manually." % (CLASS_NAMES, exc))

    # Validated + repaired centrally: the upstream PlantVillage label file ships
    # a truncated, partly-mangled list, and inventing placeholder names for the
    # gap silently breaks the web-server's /healthy/ headline match.
    sys.path.insert(0, HERE)
    from verify_class_names import load_verified

    return load_verified(CLASS_NAMES)


def to_state_dict(obj):
    import torch.nn as nn

    if isinstance(obj, nn.Module):
        sd = obj.state_dict()
    elif isinstance(obj, dict) and "state_dict" in obj:
        sd = obj["state_dict"]
    else:
        sd = obj
    # strip a DataParallel "module." prefix if present
    return {(k[7:] if k.startswith("module.") else k): v for k, v in sd.items()}


def main():
    if not os.path.exists(SRC):
        sys.exit("ERROR: source weights not found: %s" % SRC)

    import torch
    import torch.nn as nn
    from torchvision import models as tvm

    labels = load_labels()
    print("Loaded %d class labels" % len(labels))

    print("Loading %s" % SRC)
    sd = to_state_dict(torch.load(SRC, map_location="cpu"))

    # Infer the class count from the checkpoint's final Linear, and whether its
    # head is nested (classifier.1.1.*, i.e. Sequential(Dropout, Linear)) or the
    # flat torchvision default (classifier.1.*).
    nested = "classifier.1.1.weight" in sd
    lin_key = "classifier.1.1.weight" if nested else "classifier.1.weight"
    if lin_key not in sd:
        sys.exit("ERROR: no final Linear found in checkpoint (keys like %s)." % lin_key)
    num = int(sd[lin_key].shape[0])
    in_f = tvm.mobilenet_v2(pretrained=False).classifier[1].in_features
    print("checkpoint classes=%d, class_names.json labels=%d, head=%s" % (num, len(labels), "nested" if nested else "flat"))
    if num != len(labels):
        # Padding with synthetic "class_N" names used to hide this, and a bogus
        # label silently breaks the web-server's /healthy/ headline match.
        sys.exit(
            "ERROR: checkpoint has %d classes but the label file has %d.\n"
            "       Refusing to invent names -- fix models/class_names.json\n"
            "       (python3 verify_class_names.py) or point at the right checkpoint." % (num, len(labels))
        )

    # Normalize a nested head (Sequential(Dropout, Linear)) to torchvision's flat
    # Linear by REMAPPING KEYS, then load the WHOLE state dict — backbone
    # included. An earlier version built a fresh model and copied only the
    # classifier, which left a randomly-initialized feature extractor: ReLU6
    # drives it to all-zero, so every image produced identical logits (the
    # classifier bias) and scored ~1/numClasses. Never copy just the head.
    if nested:
        prefix = "classifier.1.1."
        sd = {("classifier.1." + k[len(prefix):] if k.startswith(prefix) else k): v for k, v in sd.items()}

    dst = tvm.mobilenet_v2(pretrained=False)
    dst.classifier[1] = nn.Linear(in_f, num)
    try:
        dst.load_state_dict(sd)
    except Exception as exc:
        sys.exit(
            "ERROR: state_dict didn't fit MobileNetV2 (%d classes, nested=%s): %s" % (num, nested, exc)
        )
    dst.eval()

    # Fail loudly rather than shipping a dead network (see above).
    with torch.no_grad():
        spread = float((dst.features(torch.rand(1, 3, 224, 224)) - dst.features(torch.rand(1, 3, 224, 224))).abs().max())
    if spread < 1e-6:
        sys.exit("ERROR: backbone is dead (features identical for different inputs) — refusing to save.")
    print("backbone alive (feature max|Δ| between random inputs = %.4f)" % spread)

    print("Saving legacy-format state_dict (flat head, %d classes) -> %s" % (num, OUT))
    torch.save(dst.state_dict(), OUT, _use_new_zipfile_serialization=False)

    cfg = {
        "arch": "mobilenet_v2",
        "weights": "disease.pth",
        "numClasses": num,
        "inputSize": 224,
        "mean": MEAN,
        "std": STD,
        "labels": labels,
    }
    with open(CONFIG, "w") as f:
        json.dump(cfg, f, indent=2)
    print("Wrote %s" % CONFIG)
    print("Done. Hit Analyze on the dashboard (the model lazy-loads).")


if __name__ == "__main__":
    main()
