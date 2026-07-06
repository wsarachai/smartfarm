import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Disease detection (feature 3): on-demand PlantVillage classification. The
// analysis runs only when triggered (Analyze), so there's no polling — getDisease
// is fetched on mount, and the analyze mutation primes the cache with its result.
export const diseaseApi = createApi({
  reducerPath: 'diseaseApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/disease' }),
  tagTypes: ['Disease'],
  endpoints: (builder) => ({
    getDisease: builder.query({
      query: () => '/',
      providesTags: ['Disease'],
    }),
    getDiseaseHistory: builder.query({
      query: (limit = 20) => `/history?limit=${limit}`,
      providesTags: ['Disease'],
    }),
    analyzeDisease: builder.mutation({
      query: () => ({ url: '/analyze', method: 'POST' }),
      async onQueryStarted(arg, { dispatch, queryFulfilled }) {
        try {
          const { data } = await queryFulfilled;
          dispatch(diseaseApi.util.updateQueryData('getDisease', undefined, () => data));
        } catch {
          // leave cache; the button re-enables and the user can retry
        }
      },
      invalidatesTags: ['Disease'], // refresh the history list
    }),
  }),
});

export const { useGetDiseaseQuery, useGetDiseaseHistoryQuery, useAnalyzeDiseaseMutation } = diseaseApi;
