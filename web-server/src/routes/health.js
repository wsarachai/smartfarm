const express = require('express');
const { listDevices } = require('../store/deviceStore');

const router = express.Router();

router.get('/', (req, res) => {
  res.json({
    status: 'ok',
    uptime: process.uptime(),
    deviceCount: listDevices().length,
    timestamp: new Date().toISOString(),
  });
});

module.exports = router;
