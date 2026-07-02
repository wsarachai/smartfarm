const express = require('express');
const { listDevices } = require('../store/deviceStore');

const router = express.Router();

router.get('/', (req, res) => {
  res.json(listDevices());
});

module.exports = router;
