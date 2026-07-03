import { createSlice } from '@reduxjs/toolkit';
import { analyticsApi } from './analyticsApi';

// Client-side ring buffer of recent (simulated) inference telemetry. Mirrors the
// devices historySlice pattern: fold each successful poll into a plain slice and
// trim to the most recent MAX_POINTS so memory stays bounded on the browser side.
const MAX_POINTS = 60; // ~5 min at a 5s poll

const analyticsSlice = createSlice({
  name: 'analytics',
  initialState: { latest: null, history: [] },
  reducers: {},
  extraReducers: (builder) => {
    builder.addMatcher(analyticsApi.endpoints.getAnalyticsLatest.matchFulfilled, (state, action) => {
      const payload = action.payload;
      state.latest = payload;
      state.history.push({
        t: Date.now(),
        confidencePct: payload.inference.confidencePct,
        latencyMs: payload.inference.latencyMs,
      });
      if (state.history.length > MAX_POINTS) {
        state.history.splice(0, state.history.length - MAX_POINTS);
      }
    });
  },
});

export const selectAnalyticsLatest = (state) => state.analytics.latest;
export const selectAnalyticsHistory = (state) => state.analytics.history;
export default analyticsSlice.reducer;
