import { createApi, fetchBaseQuery } from '@reduxjs/toolkit/query/react';

// Global, server-owned dashboard settings (camera source + pump). The server is
// the single source of truth (data/settings.json); every client loads the same
// object on open and a save updates it for everyone. Fetched once on mount (no
// polling — settings change rarely), re-read on window focus/reconnect, and
// invalidated after a save so all cards pick up the change immediately.
export const settingsApi = createApi({
  reducerPath: 'settingsApi',
  baseQuery: fetchBaseQuery({ baseUrl: '/api/v1/settings' }),
  tagTypes: ['Settings'],
  refetchOnFocus: true,
  refetchOnReconnect: true,
  endpoints: (builder) => ({
    getSettings: builder.query({
      query: () => '/',
      providesTags: ['Settings'],
    }),
    // Partial patch: send only the section you're editing, e.g.
    // { pump: {...} } or { cameraSource: {...} }. Server deep-merges + validates.
    updateSettings: builder.mutation({
      query: (patch) => ({
        url: '/',
        method: 'POST',
        body: patch,
      }),
      // The POST returns the full merged settings — prime the cache with it so
      // consumers update without waiting for the invalidation refetch.
      async onQueryStarted(patch, { dispatch, queryFulfilled }) {
        try {
          const { data } = await queryFulfilled;
          dispatch(
            settingsApi.util.updateQueryData('getSettings', undefined, () => data),
          );
        } catch {
          // On failure leave the cache; the form surfaces the error.
        }
      },
      invalidatesTags: ['Settings'],
    }),
  }),
});

export const { useGetSettingsQuery, useUpdateSettingsMutation } = settingsApi;
