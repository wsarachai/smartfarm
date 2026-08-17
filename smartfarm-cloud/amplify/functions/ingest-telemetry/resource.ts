import { defineFunction } from '@aws-amplify/backend';

export const ingestTelemetry = defineFunction({
  name: 'ingest-telemetry',
  entry: './handler.ts',
  timeoutSeconds: 10,
});
