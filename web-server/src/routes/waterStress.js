const express = require('express');
const waterStress = require('../insights/waterStress');
const waterStressStore = require('../store/waterStressStore');

const router = express.Router();

// Current (smoothed) water-stress estimate: { risk, band, inputs, factors, at }.
router.get('/', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(waterStress.current());
});

// Risk-over-time history (for the AI Insights trend). ?limit=N caps the points.
router.get('/history', (req, res) => {
  res.set('Cache-Control', 'no-store');
  const limit = Number(req.query.limit);
  res.json({ points: waterStressStore.list(Number.isFinite(limit) ? limit : undefined) });
});

module.exports = router;
