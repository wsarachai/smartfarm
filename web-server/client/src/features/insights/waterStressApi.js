import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Real (rule-based) water-stress estimate + its risk-over-time history. Replaces
// the old fabricated analytics simulator. Polled so the badge + trend stay live.
export const waterStressApi = createApi({
  reducerPath: 'waterStressApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/water-stress' }),
  endpoints: (builder) => ({
    getWaterStress: builder.query({
      query: () => '/',
    }),
    getWaterStressHistory: builder.query({
      query: (limit = 288) => `/history?limit=${limit}`,
    }),
  }),
});

export const { useGetWaterStressQuery, useGetWaterStressHistoryQuery } = waterStressApi;
