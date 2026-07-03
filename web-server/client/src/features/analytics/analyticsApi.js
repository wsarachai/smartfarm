import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// The AI Analytics numbers come from a backend SIMULATOR (no real inference
// engine exists in this project). The payload is clearly flagged `simulated:true`
// and every field oscillates smoothly over time — the UI labels it as fake.
export const analyticsApi = createApi({
  reducerPath: 'analyticsApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1' }),
  endpoints: (builder) => ({
    getAnalyticsLatest: builder.query({
      query: () => '/analytics/latest',
    }),
  }),
});

export const { useGetAnalyticsLatestQuery } = analyticsApi;
