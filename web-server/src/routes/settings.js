const express = require('express');
const settingsStore = require('../store/settingsStore');

const router = express.Router();

// GET /api/v1/settings -> the whole { cameraSource, pump } object.
router.get('/', (req, res) => {
  res.json(settingsStore.get());
});

// POST /api/v1/settings -> partial patch { cameraSource?, pump? }. Deep-merged by
// section, validated server-side, persisted atomically. Returns full settings.
// Each Settings-page section's Save button sends only its own section.
router.post('/', (req, res) => {
  const result = settingsStore.update(req.body || {});
  if (!result.ok) {
    return res.status(400).json({ error: result.error });
  }
  res.json(result.value);
});

module.exports = router;
