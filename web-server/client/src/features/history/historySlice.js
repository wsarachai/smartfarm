import { createSlice } from '@reduxjs/toolkit';
import { devicesApi } from '../devices/devicesApi';

// Client-side ring buffer of recent telemetry snapshots. The backend keeps only
// the latest value (no history), so the trend chart accumulates samples here on
// every successful /devices poll. Series are keyed "deviceId::metric".
const MAX_POINTS = 60; // ~5 min at a 5s poll

const historySlice = createSlice({
  name: 'history',
  initialState: { points: [] },
  reducers: {},
  extraReducers: (builder) => {
    builder.addMatcher(devicesApi.endpoints.getDevices.matchFulfilled, (state, action) => {
      const values = {};
      for (const device of action.payload) {
        for (const [key, value] of Object.entries(device.metrics || {})) {
          if (typeof value === 'number' && !Number.isNaN(value)) {
            values[`${device.device_id}::${key}`] = value;
          }
        }
      }
      // Skip empty snapshots so the chart doesn't collect blank points.
      if (Object.keys(values).length === 0) return;
      state.points.push({ t: Date.now(), values });
      if (state.points.length > MAX_POINTS) {
        state.points.splice(0, state.points.length - MAX_POINTS);
      }
    });
  },
});

export const selectHistory = (state) => state.history.points;
export default historySlice.reducer;
