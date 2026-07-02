import { configureStore } from '@reduxjs/toolkit';
import devicesReducer from '../features/devices/devicesSlice';
import { devicesApi } from '../features/devices/devicesApi';

export const store = configureStore({
  reducer: {
    devices: devicesReducer,
    [devicesApi.reducerPath]: devicesApi.reducer,
  },
  middleware: (getDefaultMiddleware) => getDefaultMiddleware().concat(devicesApi.middleware),
});
