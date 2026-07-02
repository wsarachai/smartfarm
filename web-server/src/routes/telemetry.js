const express = require('express');
const { upsertTelemetry } = require('../store/deviceStore');

const router = express.Router();

router.post('/', (req, res) => {
  const { device_id, timestamp, metrics } = req.body || {};
  if (!device_id || typeof metrics !== 'object' || metrics === null) {
    return res.status(400).json({ error: 'device_id and metrics are required' });
  }
  const device = upsertTelemetry({ device_id, timestamp, metrics });
  res.status(202).json(device);
});

module.exports = router;
