const express = require('express');
const { listDevices } = require('../store/deviceStore');
const { getHistory } = require('../store/telemetryStore');

const router = express.Router();

router.get('/', (req, res) => {
  res.json(listDevices());
});

router.get('/history', (req, res) => {
  const range = (req.query.range || 'current').toLowerCase();
  res.json(getHistory(range));
});

module.exports = router;
