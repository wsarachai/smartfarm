const express = require('express');
const pumpControl = require('../store/pumpControl');
const settingsStore = require('../store/settingsStore');

const router = express.Router();

// Thin HTTP surface over the shared pump command layer (store/pumpControl.js).
// The pump TARGET + auto-off duration are server-owned config (settingsStore),
// so the client posts only { state }. The relay + safety timer + state mirroring
// all live in pumpControl so the irrigation scheduler can drive the pump too.

// True while the irrigation scheduler owns the pump. Manual ON is refused so a
// stale browser tab can't fight the schedule; manual OFF is always allowed as an
// emergency stop (and cancels the current scheduled run's auto-off).
function autoModeOn() {
  return Boolean(settingsStore.get().irrigation.auto);
}

// GET /api/v1/pump/status  -> current state (polled). Target comes from settings.
router.get('/status', async (req, res) => {
  res.json(await pumpControl.getStatus());
});

// POST /api/v1/pump/control  body: { state:"on"|"off" }
router.post('/control', async (req, res) => {
  const { state } = req.body || {};
  if (state !== 'on' && state !== 'off') {
    return res.status(400).json({ error: 'state must be "on" or "off"' });
  }
  if (state === 'on' && autoModeOn()) {
    return res.status(409).json({ error: 'auto mode is on — manual start is disabled (switch to MANUAL first)' });
  }
  res.json(await pumpControl.command(state, { source: 'manual' }));
});

module.exports = router;
