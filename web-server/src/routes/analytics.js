const express = require('express');

const router = express.Router();

function wave(t, periodSeconds, min, max, phase = 0) {
  const mid = (min + max) / 2;
  const amp = (max - min) / 2;
  return mid + amp * Math.sin((2 * Math.PI * t) / periodSeconds + phase);
}

router.get('/latest', (req, res) => {
  const uptimeSeconds = process.uptime();

  res.json({
    simulated: true,
    timestamp: new Date().toISOString(),
    uptimeSeconds,
    model: {
      version: 'SIM-INFERENCE-v0',
      gpuClockGhz: Number(wave(uptimeSeconds, 37, 1.7, 1.95, 2.1).toFixed(2)),
    },
    inference: {
      count: Math.floor(uptimeSeconds * 2.1),
      latencyMs: Number(wave(uptimeSeconds, 29, 8, 18, 1.3).toFixed(1)),
      confidencePct: Number(wave(uptimeSeconds, 52, 85, 99).toFixed(1)),
    },
  });
});

module.exports = router;
