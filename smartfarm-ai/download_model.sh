#!/usr/bin/env bash
# Fetch a PlantVillage classifier checkpoint into models/ and write its config,
# so smartfarm-ai's /disease endpoint can load it. Run ON THE JETSON.
#
# The checkpoint must be a torch state_dict for the architecture in model_config
# below (default MobileNetV2, 38 PlantVillage classes, ImageFolder-alphabetical
# order). We can't bundle a verified checkpoint, so point DISEASE_WEIGHTS_URL at
# your source:
#   DISEASE_WEIGHTS_URL=https://…/plantvillage_mobilenetv2.pth ./download_model.sh
#
# If your checkpoint uses a different arch/label order, edit the generated
# models/model_config.json (arch, numClasses, labels) to match — the loader is
# fully config-driven.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/models"
mkdir -p "$DIR"

URL="${DISEASE_WEIGHTS_URL:-}"
if [ -z "$URL" ]; then
  echo "ERROR: set DISEASE_WEIGHTS_URL to a MobileNetV2 PlantVillage state_dict (.pth)." >&2
  echo "  e.g. DISEASE_WEIGHTS_URL=https://example/plantvillage_mobilenetv2.pth $0" >&2
  exit 1
fi

echo "Downloading weights -> $DIR/disease.pth"
curl -fL "$URL" -o "$DIR/disease.pth"

echo "Writing $DIR/model_config.json"
cat > "$DIR/model_config.json" <<'JSON'
{
  "arch": "mobilenet_v2",
  "weights": "disease.pth",
  "numClasses": 38,
  "inputSize": 224,
  "mean": [0.485, 0.456, 0.406],
  "std": [0.229, 0.224, 0.225],
  "labels": [
    "Apple___Apple_scab",
    "Apple___Black_rot",
    "Apple___Cedar_apple_rust",
    "Apple___healthy",
    "Blueberry___healthy",
    "Cherry___Powdery_mildew",
    "Cherry___healthy",
    "Corn___Cercospora_leaf_spot Gray_leaf_spot",
    "Corn___Common_rust",
    "Corn___Northern_Leaf_Blight",
    "Corn___healthy",
    "Grape___Black_rot",
    "Grape___Esca_(Black_Measles)",
    "Grape___Leaf_blight_(Isariopsis_Leaf_Spot)",
    "Grape___healthy",
    "Orange___Haunglongbing_(Citrus_greening)",
    "Peach___Bacterial_spot",
    "Peach___healthy",
    "Pepper_bell___Bacterial_spot",
    "Pepper_bell___healthy",
    "Potato___Early_blight",
    "Potato___Late_blight",
    "Potato___healthy",
    "Raspberry___healthy",
    "Soybean___healthy",
    "Squash___Powdery_mildew",
    "Strawberry___Leaf_scorch",
    "Strawberry___healthy",
    "Tomato___Bacterial_spot",
    "Tomato___Early_blight",
    "Tomato___Late_blight",
    "Tomato___Leaf_Mold",
    "Tomato___Septoria_leaf_spot",
    "Tomato___Spider_mites Two-spotted_spider_mite",
    "Tomato___Target_Spot",
    "Tomato___Tomato_Yellow_Leaf_Curl_Virus",
    "Tomato___Tomato_mosaic_virus",
    "Tomato___healthy"
  ]
}
JSON

echo "Done. Restart smartfarm-ai (or just call /disease — the model lazy-loads)."
