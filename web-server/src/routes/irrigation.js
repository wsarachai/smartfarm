const express = require('express');
const scheduler = require('../scheduler/irrigationScheduler');
const pumpLog = require('../store/pumpLog');

const router = express.Router();

// Runtime scheduler status for the Irrigation page (next run, last run/skip).
// The schedule itself (entries, auto flag, threshold, tz) is read/written via
// /api/v1/settings — this endpoint is observability only.
router.get('/status', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(scheduler.status());
});

// Pump action log (newest first). ?limit=N caps the returned rows.
router.get('/log', (req, res) => {
  res.set('Cache-Control', 'no-store');
  const limit = Number(req.query.limit);
  res.json({ entries: pumpLog.list(Number.isFinite(limit) ? limit : undefined) });
});

module.exports = router;
