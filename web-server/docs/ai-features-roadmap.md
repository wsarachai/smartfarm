# AI features roadmap

Three camera/sensor AI features, to be implemented **one at a time**. This is the
checkpoint so the deferred ones aren't lost.

Shared context: all AI **decision** logic now lives in the top-level
**`smartfarm-ai/`** container (a stdlib HTTP service, `ai_service.py`); the
web-server stays AI-agnostic and CALLS it (`AI_SERVICE_URL`). The AI container
can also pull frames via `GET /api/v1/camera/frame.jpg` (ETag/304, see
`docs/ai-frame-pull.md`). The vision features below add endpoints to `ai_service.py`
and (for pushing results back) still need a results-ingestion path.

## 1. Water Stress Estimation — sensor-first, rule-based  ✅ DONE

- **Approach:** a transparent rule engine over telemetry we ALREADY have
  (`soil_moisture` + `temperature` + `humidity`) → risk **Low / Medium / High /
  Unknown**. No trained model, no dataset, no image pipeline.
- **Decision moved to `smartfarm-ai`:** the rule now lives in
  `smartfarm-ai/water_stress.py` (served by `ai_service.py`); the web-server
  (`src/insights/waterStress.js`) aggregates telemetry, calls the AI service,
  smooths, persists history, and degrades to "AI offline" when unreachable.
- **Shipped:** `waterStressStore.js` (persisted history) + `routes/waterStress.js`;
  thresholds in `settings.json` (Settings panel); `WaterStressCard` + a real
  **AI Insights** page (`/insights`) replacing the removed analytics simulator.
- Advisory only (no pump coupling). See CLAUDE.md for the full note.
- A canopy-greenness image signal can be blended in later (depends on feature 2).

## 2. Canopy Coverage — easiest VISION feature  ⏳ LATER

- **Approach:** % green-pixel coverage via classical OpenCV color thresholding on
  the pulled frame — no trained model, just image processing on the Jetson.
- A single honest metric; a slice of the "Plant Growth Monitoring" group
  (height / leaf count / growth rate are much harder — need calibration + instance
  segmentation, so they are explicitly out of the first vision pass).
- **Requires building the results-ingestion path** (AI container POSTs results →
  server store → dashboard). This path, once built, serves feature 3 too.
- Can feed feature 1 (greenness as an extra water-stress input).

## 3. Disease Detection — hardest  ⏳ LATER

- **Approach:** image classification (healthy vs disease, e.g. leaf spot) with a
  trained / transfer-learned model (the DLI image ships ResNet transfer-learning
  notebooks).
- **Blockers:** needs a labeled dataset, accuracy validation, and **close-up leaf
  images** — the ESP32-CAM's ~1/min wide scene shots are poorly suited, so this
  likely needs a camera-placement/framing change or a dedicated capture.
- Reuses the results-ingestion path from feature 2.

## Suggested order

1 (now) → 2 (build the results path + a real vision metric) → 3 (reuse the path,
add the model + dataset). Revisit this file when starting each.
