const express = require('express');
const { applyCommand } = require('../store/deviceStore');

const router = express.Router();

router.post('/', (req, res) => {
  const { device_id, action } = req.body || {};
  if (!device_id || typeof action !== 'object' || action === null) {
    return res.status(400).json({ error: 'device_id and action are required' });
  }
  const device = applyCommand({ device_id, action });
  res.status(202).json(device);
});

module.exports = router;
