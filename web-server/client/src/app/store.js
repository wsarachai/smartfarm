import { configureStore } from '@reduxjs/toolkit';
import { setupListeners } from '@reduxjs/toolkit/query';
import devicesReducer from '../features/devices/devicesSlice';
import { devicesApi } from '../features/devices/devicesApi';
import cameraReducer from '../features/camera/cameraSlice';
import { cameraApi } from '../features/camera/cameraApi';
import historyReducer from '../features/history/historySlice';
import { healthApi } from '../features/health/healthApi';
import { pumpApi } from '../features/pump/pumpApi';
import { settingsApi } from '../features/settings/settingsApi';
import { irrigationApi } from '../features/irrigation/irrigationApi';
import { waterStressApi } from '../features/insights/waterStressApi';

export const store = configureStore({
  reducer: {
    devices: devicesReducer,
    camera: cameraReducer,
    history: historyReducer,
    [devicesApi.reducerPath]: devicesApi.reducer,
    [cameraApi.reducerPath]: cameraApi.reducer,
    [healthApi.reducerPath]: healthApi.reducer,
    [pumpApi.reducerPath]: pumpApi.reducer,
    [settingsApi.reducerPath]: settingsApi.reducer,
    [irrigationApi.reducerPath]: irrigationApi.reducer,
    [waterStressApi.reducerPath]: waterStressApi.reducer,
  },
  middleware: (getDefaultMiddleware) =>
    getDefaultMiddleware().concat(
      devicesApi.middleware,
      cameraApi.middleware,
      healthApi.middleware,
      pumpApi.middleware,
      settingsApi.middleware,
      irrigationApi.middleware,
      waterStressApi.middleware
    ),
});

// Enables refetchOnFocus / refetchOnReconnect for the settings query so
// already-open tablets pick up a change made elsewhere without a manual refresh.
setupListeners(store.dispatch);
