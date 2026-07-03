import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Backs the status header (server uptime / device count / liveness).
export const healthApi = createApi({
  reducerPath: 'healthApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1' }),
  endpoints: (builder) => ({
    getHealth: builder.query({
      query: () => '/health',
    }),
  }),
});

export const { useGetHealthQuery } = healthApi;
