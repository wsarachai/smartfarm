import { type ClientSchema, a, defineData } from '@aws-amplify/backend';
import { dashboardApi } from '../functions/dashboard-api/resource';

// No `a.model()` here on purpose: the telemetry table needs DynamoDB TTL,
// which Amplify Data's model-backed tables don't expose, so that table is a
// raw CDK construct in backend.ts instead. This schema only carries the two
// custom operations the dashboard needs, each backed by dashboard-api's
// Lambda and gated by the same Cognito user pool as everything else.
const schema = a.schema({
  TelemetryPoint: a.customType({
    hubId: a.string().required(),
    deviceId: a.string().required(),
    timestamp: a.string().required(),
    metrics: a.json(),
  }),

  getTelemetryHistory: a
    .query()
    .arguments({
      hubId: a.string().required(),
      hours: a.integer(),
    })
    .returns(a.ref('TelemetryPoint').array())
    .authorization((allow) => [allow.authenticated()])
    .handler(a.handler.function(dashboardApi)),

  sendPumpCommand: a
    .mutation()
    .arguments({
      hubId: a.string().required(),
      deviceId: a.string().required(),
      state: a.string().required(),
    })
    .returns(a.string())
    .authorization((allow) => [allow.authenticated()])
    .handler(a.handler.function(dashboardApi)),
});

export type Schema = ClientSchema<typeof schema>;

export const data = defineData({
  schema,
  authorizationModes: {
    defaultAuthorizationMode: 'userPool',
  },
});
