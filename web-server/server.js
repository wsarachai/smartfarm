const path = require('path');
const express = require('express');

const telemetryRouter = require('./src/routes/telemetry');
const controlRouter = require('./src/routes/control');
const devicesRouter = require('./src/routes/devices');
const healthRouter = require('./src/routes/health');
const cameraRouter = require('./src/routes/camera');

const app = express();
const PORT = process.env.PORT || 3000;
const CLIENT_BUILD_DIR = path.join(__dirname, 'client', 'dist');

app.use(express.json());

app.use('/api/v1/telemetry', telemetryRouter);
app.use('/api/v1/control', controlRouter);
app.use('/api/v1/devices', devicesRouter);
app.use('/api/v1/health', healthRouter);
app.use('/api/v1/camera', cameraRouter);

app.use(express.static(CLIENT_BUILD_DIR));

app.get('*', (req, res) => {
  res.sendFile(path.join(CLIENT_BUILD_DIR, 'index.html'));
});

app.listen(PORT, () => {
  console.log(`Smart Farm Control Center listening on port ${PORT}`);
});
