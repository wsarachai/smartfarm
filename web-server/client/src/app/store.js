import { configureStore } from '@reduxjs/toolkit';
import devicesReducer from '../features/devices/devicesSlice';
import { devicesApi } from '../features/devices/devicesApi';
import cameraReducer from '../features/camera/cameraSlice';
import { cameraApi } from '../features/camera/cameraApi';
import historyReducer from '../features/history/historySlice';
import { healthApi } from '../features/health/healthApi';
import analyticsReducer from '../features/analytics/analyticsSlice';
import { analyticsApi } from '../features/analytics/analyticsApi';

export const store = configureStore({
  reducer: {
    devices: devicesReducer,
    camera: cameraReducer,
    history: historyReducer,
    analytics: analyticsReducer,
    [devicesApi.reducerPath]: devicesApi.reducer,
    [cameraApi.reducerPath]: cameraApi.reducer,
    [healthApi.reducerPath]: healthApi.reducer,
    [analyticsApi.reducerPath]: analyticsApi.reducer,
  },
  middleware: (getDefaultMiddleware) =>
    getDefaultMiddleware().concat(
      devicesApi.middleware,
      cameraApi.middleware,
      healthApi.middleware,
      analyticsApi.middleware
    ),
});
