import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Talks to the backend pump relay (never the pump directly — CORS). getPumpStatus
// is polled to keep the card truthful (reflects backend-fired auto-off / offline
// within one interval); setPump issues on/off and re-arms the safety timer.
export const pumpApi = createApi({
  reducerPath: 'pumpApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/pump' }),
  endpoints: (builder) => ({
    getPumpStatus: builder.query({
      query: (target) => `/status?target=${encodeURIComponent(target)}`,
    }),
    setPump: builder.mutation({
      query: ({ target, state, autoOffMinutes }) => ({
        url: '/control',
        method: 'POST',
        body: { target, state, autoOffMinutes },
      }),
      // A command changes the pump; refresh the polled status immediately.
      async onQueryStarted(arg, { dispatch, queryFulfilled }) {
        try {
          const { data } = await queryFulfilled;
          dispatch(
            pumpApi.util.updateQueryData('getPumpStatus', arg.target, () => data),
          );
        } catch {
          // Leave the polled status as-is; the next poll will reconcile.
        }
      },
    }),
  }),
});

export const { useGetPumpStatusQuery, useSetPumpMutation } = pumpApi;
