import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Talks to the backend pump relay (never the pump directly — CORS). The pump
// TARGET + auto-off duration are server-owned config (settings.json), so the
// client sends only { state }. getPumpStatus is polled to keep the card truthful
// (reflects backend-fired auto-off / offline within one interval); setPump issues
// on/off and re-arms the safety timer.
export const pumpApi = createApi({
  reducerPath: 'pumpApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/pump' }),
  endpoints: (builder) => ({
    getPumpStatus: builder.query({
      query: () => '/status',
    }),
    setPump: builder.mutation({
      query: ({ state }) => ({
        url: '/control',
        method: 'POST',
        body: { state },
      }),
      // A command changes the pump; refresh the polled status immediately.
      async onQueryStarted(arg, { dispatch, queryFulfilled }) {
        try {
          const { data } = await queryFulfilled;
          dispatch(
            pumpApi.util.updateQueryData('getPumpStatus', undefined, () => data),
          );
        } catch {
          // Leave the polled status as-is; the next poll will reconcile.
        }
      },
    }),
  }),
});

export const { useGetPumpStatusQuery, useSetPumpMutation } = pumpApi;
