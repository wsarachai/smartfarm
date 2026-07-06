const express = require('express');
const canopy = require('../insights/canopy');
const canopyStore = require('../store/canopyStore');

const router = express.Router();

// Current (smoothed) canopy estimate: { canopyPercent, factors, at, aiOnline, ... }.
// Deliberately light (no image) so the UI can poll it cheaply.
router.get('/', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(canopy.current());
});

// Canopy-over-time history (for the AI Insights trend). ?limit=N caps the points.
router.get('/history', (req, res) => {
  res.set('Cache-Control', 'no-store');
  const limit = Number(req.query.limit);
  res.json({ points: canopyStore.list(Number.isFinite(limit) ? limit : undefined) });
});

// Latest green-mask preview (RAM-only debug view for tuning HSV thresholds).
router.get('/preview.png', (req, res) => {
  const png = canopy.previewPng();
  if (!png) return res.status(503).json({ error: 'no canopy preview yet' });
  res.set('Content-Type', 'image/png');
  res.set('Cache-Control', 'no-store');
  res.send(png);
});

module.exports = router;
