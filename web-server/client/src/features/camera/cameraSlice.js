import { createSlice } from '@reduxjs/toolkit';
import { cameraApi } from './cameraApi';

const initialState = {
  online: false,
  hasFrame: false,
  ageMs: null,
  bytes: 0,
  receivedAt: null,
  degrading: false,
};

// Mirrors the pattern used by devicesSlice: the polled RTK Query result is
// folded into a plain slice via matchFulfilled, giving components a stable
// selector to read instead of the query cache directly.
const cameraSlice = createSlice({
  name: 'camera',
  initialState,
  reducers: {},
  extraReducers: (builder) => {
    builder.addMatcher(cameraApi.endpoints.getCameraStatus.matchFulfilled, (state, action) => {
      Object.assign(state, action.payload);
    });
  },
});

export const selectCameraStatus = (state) => state.camera;
export default cameraSlice.reducer;
