import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// The frame pixels are delivered straight to an <img> via /stream — Redux only
// tracks lightweight status (online/stale, age, size) so the UI can badge it.
export const cameraApi = createApi({
  reducerPath: 'cameraApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/camera' }),
  endpoints: (builder) => ({
    getCameraStatus: builder.query({
      query: () => '/status',
    }),
  }),
});

export const { useGetCameraStatusQuery } = cameraApi;
