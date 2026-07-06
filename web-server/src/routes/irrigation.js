const express = require('express');
const scheduler = require('../scheduler/irrigationScheduler');

const router = express.Router();

// Runtime scheduler status for the Irrigation page (next run, last run/skip).
// The schedule itself (entries, auto flag, threshold, tz) is read/written via
// /api/v1/settings — this endpoint is observability only.
router.get('/status', (req, res) => {
  res.set('Cache-Control', 'no-store');
  res.json(scheduler.status());
});

module.exports = router;
