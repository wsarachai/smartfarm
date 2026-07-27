#!/usr/bin/env python3
"""Convert the PlantVillage MobileNetV2 checkpoint to TFLite, for armv7l hosts.

WHY: PyTorch publishes no 32-bit ARM wheels (zero armv7l files on PyPI, any
version), so a Raspberry Pi 3 B cannot run the .pth model that the Jetson uses.
tflite-runtime does ship armv7l wheels, so the Pi runs the same network through
the TFLite interpreter instead (see disease.py's two backends).

RUN THIS OFF-PI — on an x86_64/aarch64 dev box. It needs torch + tensorflow +
onnx2tf (~2 GB of wheels) which the Pi can neither install nor afford; the Pi
only ever consumes the ~9 MB .tflite output.

    python3 -m venv .venv && . .venv/bin/activate
    pip install torch torchvision onnx onnx2tf tensorflow \
                onnx-graphsurgeon sng4onnx simple_onnx_processing_tools psutil \
                --extra-index-url https://pypi.ngc.nvidia.com
    python3 convert_to_tflite.py

Reads  models/mobilenetv2_plant.pth (+ models/class_names.json)
Writes models/disease.tflite + models/model_config.json  (runtime: "tflite")

Then copy BOTH to the Pi's smartfarm-ai/models/ and restart the container.
The pipeline is torch -> ONNX -> TFLite: onnx2tf does the NCHW->NHWC transpose
that TFLite requires, which is the part a hand-rolled Keras port gets wrong
(torchvision pads symmetrically, Keras pads TF-style, so the two disagree).

Env overrides: DISEASE_SRC_WEIGHTS, DISEASE_CLASS_NAMES, DISEASE_SAMPLE_IMAGE.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
MODELS = os.path.join(HERE, "models")
SRC = os.environ.get("DISEASE_SRC_WEIGHTS", os.path.join(MODELS, "mobilenetv2_plant.pth"))
OUT_TFLITE = os.path.join(MODELS, "disease.tflite")
CONFIG = os.path.join(MODELS, "model_config.json")
CLASS_NAMES = os.environ.get("DISEASE_CLASS_NAMES", os.path.join(MODELS, "class_names.json"))
SAMPLE = os.environ.get("DISEASE_SAMPLE_IMAGE", os.path.join(MODELS, "leaf.jpg"))
INPUT_SIZE = 224
MEAN = [0.485, 0.456, 0.406]
STD = [0.229, 0.224, 0.225]


def load_labels():
    """Ordered class names. Shares convert_weights.py's shape-tolerant parsing:
    a list, an index->name dict, or a name->index dict."""
    if not os.path.exists(CLASS_NAMES):
        sys.exit("ERROR: no %s — run download_model.sh first." % CLASS_NAMES)
    with open(CLASS_NAMES) as f:
        data = json.load(f)

    if isinstance(data, list):
        return [str(x) for x in data]
    if isinstance(data, dict):
        keys = list(data.keys())
        if keys and all(str(k).lstrip("-").isdigit() for k in keys):
            return [str(data[k]) for k in sorted(keys, key=lambda k: int(k))]
        try:
            inv = {int(v): k for k, v in data.items()}
            return [inv[i] for i in range(len(inv))]
        except Exception:
            return [str(k) for k in keys]
    sys.exit("ERROR: unrecognized class_names.json shape")


def build_torch_model(labels):
    """Load the checkpoint into a torchvision MobileNetV2 with a FLAT head.
    Mirrors convert_weights.py so both converters agree on label order."""
    import torch
    import torch.nn as nn
    from torchvision import models as tvm

    if not os.path.exists(SRC):
        sys.exit("ERROR: source weights not found: %s" % SRC)

    obj = torch.load(SRC, map_location="cpu")
    if isinstance(obj, nn.Module):
        sd = obj.state_dict()
    elif isinstance(obj, dict) and "state_dict" in obj:
        sd = obj["state_dict"]
    else:
        sd = obj
    sd = {(k[7:] if k.startswith("module.") else k): v for k, v in sd.items()}

    # The checkpoint's head is either nested (Sequential(Dropout, Linear)) or
    # torchvision's flat Linear; detect which so load_state_dict fits.
    nested = "classifier.1.1.weight" in sd
    lin_key = "classifier.1.1.weight" if nested else "classifier.1.weight"
    if lin_key not in sd:
        sys.exit("ERROR: no final Linear found in checkpoint (expected %s)." % lin_key)
    num = int(sd[lin_key].shape[0])
    in_f = tvm.mobilenet_v2(weights=None).classifier[1].in_features
    print("checkpoint classes=%d, labels=%d, head=%s" % (num, len(labels), "nested" if nested else "flat"))

    if num != len(labels):
        print("WARNING: class count mismatch (%d vs %d) — using %d; verify label order." % (num, len(labels), num))
        labels = (labels + ["class_%d" % i for i in range(len(labels), num)])[:num]

    # Normalize a nested head (Sequential(Dropout, Linear)) to torchvision's flat
    # Linear by REMAPPING KEYS — so the trained BACKBONE loads too. Rebuilding a
    # fresh model and copying only the classifier leaves a random feature
    # extractor, which ReLU6 drives to all-zero and makes every image score
    # identically (the bug in convert_weights.py). Never copy just the head.
    if nested:
        prefix = "classifier.1.1."
        sd = {("classifier.1." + k[len(prefix):] if k.startswith(prefix) else k): v for k, v in sd.items()}

    dst = tvm.mobilenet_v2(weights=None)
    dst.classifier[1] = nn.Linear(in_f, num)
    try:
        dst.load_state_dict(sd)
    except Exception as exc:
        sys.exit("ERROR: state_dict didn't fit MobileNetV2 (%d classes, nested=%s): %s" % (num, nested, exc))
    dst.eval()
    assert_backbone_alive(dst)
    return dst, labels, num


def assert_backbone_alive(model):
    """Guard against a dead/untrained feature extractor: two different inputs
    must produce different features. Catches the silent 'random backbone' bug
    that otherwise only shows up as every frame scoring ~1/numClasses."""
    import torch

    with torch.no_grad():
        a = model.features(torch.rand(1, 3, INPUT_SIZE, INPUT_SIZE))
        b = model.features(torch.rand(1, 3, INPUT_SIZE, INPUT_SIZE))
    spread = float((a - b).abs().max())
    if spread < 1e-6:
        sys.exit(
            "ERROR: backbone is dead (features identical for different inputs, max|Δ|=%g).\n"
            "       The checkpoint's feature weights did not load — refusing to ship this model." % spread
        )
    print("backbone alive (feature max|Δ| between random inputs = %.4f)" % spread)


def export_onnx(model, path):
    import torch

    dummy = torch.zeros(1, 3, INPUT_SIZE, INPUT_SIZE)
    print("Exporting ONNX -> %s" % path)
    kw = dict(input_names=["input"], output_names=["logits"], opset_version=13, do_constant_folding=True)
    try:
        # torch >=2.9 defaults to the dynamo exporter, which needs onnxscript and
        # emits a graph onnx2tf handles less predictably. Ask for the legacy one.
        torch.onnx.export(model, dummy, path, dynamo=False, **kw)
    except TypeError:
        torch.onnx.export(model, dummy, path, **kw)  # older torch: no dynamo kwarg


def onnx_to_tflite(onnx_path, workdir):
    """onnx2tf emits a folder of variants; we want the float32 one (the Pi has
    no NPU, and float32 avoids quantization accuracy loss on an already-marginal
    wide-shot classifier)."""
    print("Converting ONNX -> TFLite (onnx2tf)…")
    try:
        import onnx2tf

        onnx2tf.convert(input_onnx_file_path=onnx_path, output_folder_path=workdir, non_verbose=True)
    except ImportError:
        subprocess.check_call(["onnx2tf", "-i", onnx_path, "-o", workdir, "-n"])

    for name in ("model_float32.tflite", os.path.basename(onnx_path).replace(".onnx", "_float32.tflite")):
        cand = os.path.join(workdir, name)
        if os.path.exists(cand):
            return cand
    produced = [f for f in os.listdir(workdir) if f.endswith(".tflite")]
    if not produced:
        sys.exit("ERROR: onnx2tf produced no .tflite in %s" % workdir)
    return os.path.join(workdir, sorted(produced)[0])


def verify(tflite_path, torch_model, labels):
    """Run both models on a sample leaf and compare — a silent NCHW/NHWC or
    label-order mistake shows up here, not in the field."""
    if not os.path.exists(SAMPLE):
        print("(no %s — skipping verification)" % os.path.basename(SAMPLE))
        return
    import numpy as np
    import torch
    from PIL import Image

    img = Image.open(SAMPLE).convert("RGB").resize((INPUT_SIZE, INPUT_SIZE), Image.BILINEAR)
    x = (np.asarray(img, dtype=np.float32) / 255.0 - np.array(MEAN, np.float32)) / np.array(STD, np.float32)

    with torch.no_grad():
        t_logits = torch_model(torch.from_numpy(x.transpose(2, 0, 1)).unsqueeze(0))[0].numpy()

    sys.path.insert(0, HERE)
    from disease import _find_interpreter  # same fallback chain the Pi uses

    interp = _find_interpreter()(model_path=tflite_path)
    interp.allocate_tensors()
    inp, out = interp.get_input_details()[0], interp.get_output_details()[0]
    interp.set_tensor(inp["index"], np.expand_dims(x, 0).astype(np.float32))
    interp.invoke()
    l_logits = interp.get_tensor(out["index"])[0]

    t_top, l_top = int(t_logits.argmax()), int(l_logits.argmax())
    delta = float(np.abs(t_logits - l_logits).max())
    print("verify: torch top1=%s | tflite top1=%s | max|Δlogit|=%.4f" % (labels[t_top], labels[l_top], delta))
    if t_top != l_top or delta > 1e-2:
        print("WARNING: outputs disagree — do NOT ship this model; check the onnx2tf version.")
    else:
        print("verify: OK — the TFLite model matches the torch model.")


def main():
    labels = load_labels()
    print("Loaded %d class labels" % len(labels))
    model, labels, num = build_torch_model(labels)

    workdir = tempfile.mkdtemp(prefix="tflite-convert-")
    try:
        onnx_path = os.path.join(workdir, "disease.onnx")
        export_onnx(model, onnx_path)
        produced = onnx_to_tflite(onnx_path, os.path.join(workdir, "tf"))
        shutil.copyfile(produced, OUT_TFLITE)
        print("Wrote %s (%.1f MB)" % (OUT_TFLITE, os.path.getsize(OUT_TFLITE) / 1e6))
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    cfg = {
        "runtime": "tflite",
        "arch": "mobilenet_v2",
        "weights": "disease.tflite",
        "numClasses": num,
        "inputSize": INPUT_SIZE,
        "mean": MEAN,
        "std": STD,
        "applySoftmax": True,
        "labels": labels,
    }
    with open(CONFIG, "w") as f:
        json.dump(cfg, f, indent=2)
    print("Wrote %s" % CONFIG)

    verify(OUT_TFLITE, model, labels)
    print("\nDone. Copy models/disease.tflite + models/model_config.json to the Pi, then:")
    print("  docker compose -f docker-compose.rpi.yaml up -d --build   # picks up tflite-runtime")


if __name__ == "__main__":
    main()
