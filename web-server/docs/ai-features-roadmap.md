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

## 2. Canopy Coverage — easiest VISION feature  ✅ DONE

- **Approach:** % green-pixel coverage via classical **HSV** thresholding (PIL +
  numpy, no cv2) on the latest frame — no trained model. A single honest metric;
  a slice of "Plant Growth Monitoring" (height / leaf count / growth rate remain
  much harder — calibration + instance segmentation — and are out of this pass).
- **Decision in `smartfarm-ai`:** `smartfarm-ai/canopy.py` (served by `ai_service.py`
  `POST /canopy`, raw JPEG + HSV params as query) returns `{canopyPercent, factors,
  maskPng}`. Uses the **orchestrator** pattern (web-server POSTs its latest frame),
  not a results-ingestion path — consistent with water stress.
- **Shipped:** web-server `src/insights/canopy.js` (orchestrator) + `canopyStore.js`
  (persisted history) + `routes/canopy.js` (`/`, `/history`, `/preview.png` mask);
  HSV thresholds in `settings.json` `canopy` (Settings panel, live-tunable while
  watching the mask); a Canopy panel + trend on the AI Insights page. Graceful
  degrade (no fresh frame → unknown; AI down → AI OFFLINE).
- Greenness could feed feature 1 (water stress) as an extra input later.

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
