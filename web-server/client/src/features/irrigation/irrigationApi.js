import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Runtime status of the AUTO-mode scheduler (next run, last run, last skip).
// The schedule ITSELF (entries, auto flag, threshold, tz) lives in settingsApi;
// this is observability only, polled so the Irrigation page stays truthful.
export const irrigationApi = createApi({
  reducerPath: 'irrigationApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/irrigation' }),
  endpoints: (builder) => ({
    getIrrigationStatus: builder.query({
      query: () => '/status',
    }),
  }),
});

export const { useGetIrrigationStatusQuery } = irrigationApi;
