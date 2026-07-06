const path = require('path');
const express = require('express');

const telemetryRouter = require('./src/routes/telemetry');
const controlRouter = require('./src/routes/control');
const devicesRouter = require('./src/routes/devices');
const healthRouter = require('./src/routes/health');
const cameraRouter = require('./src/routes/camera');
const pumpRouter = require('./src/routes/pump');
const settingsRouter = require('./src/routes/settings');
const irrigationRouter = require('./src/routes/irrigation');
const waterStressRouter = require('./src/routes/waterStress');
const cameraConfig = require('./src/store/cameraConfig');
const settingsStore = require('./src/store/settingsStore');
const pumpLog = require('./src/store/pumpLog');
const waterStressStore = require('./src/store/waterStressStore');
const irrigationScheduler = require('./src/scheduler/irrigationScheduler');
const waterStress = require('./src/insights/waterStress');

const app = express();
const PORT = process.env.PORT || 3000;
const CLIENT_BUILD_DIR = path.join(__dirname, 'client', 'dist');

app.use(express.json());

app.use('/api/v1/telemetry', telemetryRouter);
app.use('/api/v1/control', controlRouter);
app.use('/api/v1/devices', devicesRouter);
app.use('/api/v1/health', healthRouter);
app.use('/api/v1/camera', cameraRouter);
app.use('/api/v1/pump', pumpRouter);
app.use('/api/v1/settings', settingsRouter);
app.use('/api/v1/irrigation', irrigationRouter);
app.use('/api/v1/water-stress', waterStressRouter);

app.use(express.static(CLIENT_BUILD_DIR));

app.get('*', (req, res) => {
  res.sendFile(path.join(CLIENT_BUILD_DIR, 'index.html'));
});

// Load persisted config (camera device config + dashboard settings) before serving.
// cameraConfig also sizes the frame ring to match its persisted ring_size.
cameraConfig.load();
settingsStore.load();
pumpLog.load(); // restore the pump action log so history survives restart
waterStressStore.load(); // restore water-stress risk history

// Start the AUTO-mode irrigation scheduler (reads the schedule from settings
// each tick; idle until irrigation.auto is enabled).
irrigationScheduler.start();
// Start the water-stress estimator (advisory; recomputes from live telemetry).
waterStress.start();

app.listen(PORT, () => {
  console.log(`Smart Farm Control Center listening on port ${PORT}`);
});
