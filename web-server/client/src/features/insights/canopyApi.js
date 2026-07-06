import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Canopy coverage (feature 2): % green-pixel cover, computed in smartfarm-ai and
// orchestrated by the web-server. Polled for the AI Insights panel; the mask
// preview image is a separate endpoint (/preview.png), not fetched here.
export const canopyApi = createApi({
  reducerPath: 'canopyApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/canopy' }),
  endpoints: (builder) => ({
    getCanopy: builder.query({
      query: () => '/',
    }),
    getCanopyHistory: builder.query({
      query: (limit = 288) => `/history?limit=${limit}`,
    }),
  }),
});

export const { useGetCanopyQuery, useGetCanopyHistoryQuery } = canopyApi;
