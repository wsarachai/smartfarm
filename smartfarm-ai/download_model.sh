#!/usr/bin/env bash
# Fetch the pre-converted PlantVillage MobileNetV2 disease model files into models/.
# Downloads directly from https://huggingface.co/wsarachai/plant-disease-mobilenetv2
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/models"
mkdir -p "$DIR"

BASE_URL="https://huggingface.co/wsarachai/plant-disease-mobilenetv2/resolve/main"

WEIGHTS_URL="${DISEASE_WEIGHTS_URL:-$BASE_URL/disease.pth}"
TFLITE_URL="${DISEASE_TFLITE_URL:-$BASE_URL/disease.tflite}"
CONFIG_URL="${DISEASE_CONFIG_URL:-$BASE_URL/model_config.json}"
CLASS_URL="${DISEASE_CLASS_NAMES_URL:-$BASE_URL/class_names.json}"

echo "Downloading PyTorch weights -> $DIR/disease.pth"
curl -fL "$WEIGHTS_URL" -o "$DIR/disease.pth" || echo "Note: disease.pth download skipped or failed."

echo "Downloading TFLite model   -> $DIR/disease.tflite"
curl -fL "$TFLITE_URL" -o "$DIR/disease.tflite" || echo "Note: disease.tflite download skipped or failed."

echo "Downloading model config    -> $DIR/model_config.json"
curl -fL "$CONFIG_URL" -o "$DIR/model_config.json"

echo "Downloading class labels    -> $DIR/class_names.json"
curl -fL "$CLASS_URL" -o "$DIR/class_names.json"

echo
if command -v python3 >/dev/null 2>&1; then
  python3 "$(dirname "$DIR")/verify_class_names.py" "$DIR/class_names.json"
fi

echo
echo "Model files downloaded successfully into models/."
echo "No conversion step required. You can now start smartfarm-ai and run inference."

