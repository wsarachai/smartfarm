import { createSlice } from '@reduxjs/toolkit';
import { devicesApi } from './devicesApi';

const initialState = {
  byId: {},
};

const devicesSlice = createSlice({
  name: 'devices',
  initialState,
  reducers: {
    telemetryReceived(state, action) {
      const { device_id, metrics } = action.payload;
      const existing = state.byId[device_id] || { device_id, type: 'sensor', metrics: {} };
      existing.metrics = { ...existing.metrics, ...metrics };
      state.byId[device_id] = existing;
    },
    actuatorStateChanged(state, action) {
      const { device_id, action: commandAction } = action.payload;
      const existing = state.byId[device_id] || { device_id, type: 'actuator', metrics: {} };
      existing.type = 'actuator';
      existing.metrics = { ...existing.metrics, ...commandAction };
      state.byId[device_id] = existing;
    },
  },
  extraReducers: (builder) => {
    builder.addMatcher(devicesApi.endpoints.getDevices.matchFulfilled, (state, action) => {
      state.byId = Object.fromEntries(action.payload.map((device) => [device.device_id, device]));
    });
  },
});

export const { telemetryReceived, actuatorStateChanged } = devicesSlice.actions;
export const selectAllDevices = (state) => Object.values(state.devices.byId);
export default devicesSlice.reducer;
