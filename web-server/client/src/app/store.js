import { configureStore } from '@reduxjs/toolkit';
import devicesReducer from '../features/devices/devicesSlice';
import { devicesApi } from '../features/devices/devicesApi';
import cameraReducer from '../features/camera/cameraSlice';
import { cameraApi } from '../features/camera/cameraApi';

export const store = configureStore({
  reducer: {
    devices: devicesReducer,
    camera: cameraReducer,
    [devicesApi.reducerPath]: devicesApi.reducer,
    [cameraApi.reducerPath]: cameraApi.reducer,
  },
  middleware: (getDefaultMiddleware) =>
    getDefaultMiddleware().concat(devicesApi.middleware, cameraApi.middleware),
});
