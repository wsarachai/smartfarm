#!/usr/bin/env bash
# Fetch the PlantVillage MobileNetV2 checkpoint + its class names into models/,
# then convert them for the /disease endpoint. Run ON THE JETSON.
#
# Defaults target Daksh159/plant-disease-mobilenetv2 (torchvision MobileNetV2,
# 38 classes, ImageNet preprocessing). Override with DISEASE_WEIGHTS_URL /
# DISEASE_CLASS_NAMES_URL for a different checkpoint (must be a torchvision
# MobileNetV2 state_dict; edit numClasses/labels in convert_weights.py if it
# differs).
#
# Two steps: (1) this script downloads the raw files (curl, host-side), then
# (2) convert_weights.py normalizes them INSIDE the container (needs torch):
#     ./download_model.sh
#     docker exec smartfarm-ai python3 /smartfarm-ai/convert_weights.py
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/models"
mkdir -p "$DIR"

WEIGHTS_URL="${DISEASE_WEIGHTS_URL:-https://huggingface.co/Daksh159/plant-disease-mobilenetv2/resolve/main/mobilenetv2_plant.pth}"
CLASS_URL="${DISEASE_CLASS_NAMES_URL:-https://huggingface.co/Daksh159/plant-disease-mobilenetv2/resolve/main/class_names.json}"

echo "Downloading weights  -> $DIR/mobilenetv2_plant.pth"
curl -fL "$WEIGHTS_URL" -o "$DIR/mobilenetv2_plant.pth"
echo "Downloading labels   -> $DIR/class_names.json"
curl -fL "$CLASS_URL" -o "$DIR/class_names.json"

echo
echo "Downloaded. Now convert (inside the running smartfarm-ai container):"
echo "  docker exec smartfarm-ai python3 /smartfarm-ai/convert_weights.py"
echo "That writes models/disease.pth + models/model_config.json; then hit Analyze."
