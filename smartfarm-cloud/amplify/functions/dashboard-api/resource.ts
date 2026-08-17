import { defineFunction } from '@aws-amplify/backend';

export const dashboardApi = defineFunction({
  name: 'dashboard-api',
  entry: './handler.ts',
  timeoutSeconds: 15,
});
