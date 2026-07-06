const express = require('express');
const disease = require('../insights/disease');
const diseaseStore = require('../store/diseaseStore');

const router = express.Router();

// Last analysis result (on-demand; may be 'idle' until first Analyze).
router.get('/', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(disease.current());
});

// Trigger an analysis of the latest camera frame (heavy; de-duped server-side).
router.post('/analyze', async (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(await disease.analyze());
});

// Recent-checks log (newest first). ?limit=N caps the entries.
router.get('/history', (req, res) => {
  res.set('Cache-Control', 'no-store');
  const limit = Number(req.query.limit);
  res.json({ entries: diseaseStore.list(Number.isFinite(limit) ? limit : undefined) });
});

module.exports = router;
