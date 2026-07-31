---
license: apache-2.0
language:
- en
metrics:
- accuracy
pipeline_tag: image-classification
tags:
- plant-disease
- mobilenetv2
- image-classification
- computer-vision
- pytorch
- tflite
---

# Plant Disease Classification – MobileNetV2

This model card describes the **MobileNetV2 Plant Disease Classifier** prepared for upload to [huggingface.co](https://huggingface.co).

## 👤 Original Author & Attribution

- **Original Model & Weights:** [Daksh159/plant-disease-mobilenetv2](https://huggingface.co/Daksh159/plant-disease-mobilenetv2)
- **Original Author:** Daksh Goyal

---

## 📊 Model Summary & Specifications

- **Architecture:** MobileNetV2
- **Dataset:** PlantVillage (Augmented dataset, ~87,000 images)
- **Total Classes:** 38 crop disease and healthy plant classes
- **Input Dimensions:** `224 × 224` (RGB)
- **Preprocessing:** ImageNet normalization (`mean=[0.485, 0.456, 0.406]`, `std=[0.229, 0.224, 0.225]`)

---

## 📐 Input & Output Tensor Specifications

### 1. PyTorch Format (`mobilenetv2_plant.pth` / `disease.pth`)
- **Framework:** PyTorch
- **Input Tensor Shape:** `[Batch_Size, 3, 224, 224]` (Float32, NCHW layout)
- **Output Tensor Shape:** `[Batch_Size, 38]` (Raw Logits / Softmax probabilities)

### 2. TensorFlow Lite Format (`disease.tflite`)
- **Framework:** TensorFlow Lite
- **Input Tensor Shape:** `[Batch_Size, 224, 224, 3]` (Float32, NHWC layout)
- **Output Tensor Shape:** `[Batch_Size, 38]` (Softmax probabilities)

---

## 🏷 Supported Classes (38 Classes)

The model classifies 38 disease and healthy states across crops including Apple, Blueberry, Cherry, Corn, Grape, Peach, Pepper, Potato, Raspberry, Soybean, Squash, Strawberry, Tomato, and Orange. 

Class label mappings are stored in `class_names.json` and `model_config.json`.