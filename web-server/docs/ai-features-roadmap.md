# AI features roadmap

Three camera/sensor AI features, to be implemented **one at a time**. This is the
checkpoint so the deferred ones aren't lost.

Shared context: the app's "AI Analytics" page + insight card are currently a
**simulator** (`src/routes/analytics.js`, fabricated telemetry). The AI container
(NVIDIA DLI image) can pull frames via `GET /api/v1/camera/frame.jpg` (ETag/304,
see `docs/ai-frame-pull.md`), but there is **no results-ingestion path** yet
(AI → server → dashboard). The two vision features below need that path; the
first one does not.

## 1. Water Stress Estimation — sensor-first, rule-based  ✅ DONE

- **Approach:** a transparent rule engine over telemetry we ALREADY have
  (`soil_moisture` + `temperature` + `humidity`) → risk **Low / Medium / High /
  Unknown**. No trained model, no dataset, no image pipeline. Runs server-side.
- **Shipped:** `src/insights/waterStress.js` + `waterStressStore.js` (persisted
  history) + `routes/waterStress.js`; thresholds in `settings.json` (Settings
  page panel); dashboard `WaterStressCard`; the old analytics simulator was
  removed and `/analytics` became the real **AI Insights** page (`/insights`).
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
