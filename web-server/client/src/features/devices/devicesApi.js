import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

export const devicesApi = createApi({
  reducerPath: 'devicesApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1' }),
  endpoints: (builder) => ({
    getDevices: builder.query({
      query: () => '/devices',
    }),
    getTelemetryHistory: builder.query({
      query: (range = 'current') => `/devices/history?range=${range}`,
    }),
    sendCommand: builder.mutation({
      query: (body) => ({
        url: '/control',
        method: 'POST',
        body,
      }),
    }),
  }),
});

export const { useGetDevicesQuery, useGetTelemetryHistoryQuery, useSendCommandMutation } = devicesApi;
