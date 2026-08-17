// Single hub for v1, but every read/write already takes hubId as a parameter
// (see amplify/data/resource.ts) — adding hub #2 later is picking a different
// value here (or a hub picker UI), not a schema or API change.
export const HUB_ID = process.env.NEXT_PUBLIC_HUB_ID ?? 'default-hub';

export const PUMP_DEVICE_ID = 'main-pump';
