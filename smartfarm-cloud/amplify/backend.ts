import { defineBackend } from '@aws-amplify/backend';
import { AttributeType, BillingMode, Table } from 'aws-cdk-lib/aws-dynamodb';
import { CfnPolicy, CfnTopicRule } from 'aws-cdk-lib/aws-iot';
import { Effect, PolicyStatement, ServicePrincipal } from 'aws-cdk-lib/aws-iam';
import { auth } from './auth/resource';
import { data } from './data/resource';
import { ingestTelemetry } from './functions/ingest-telemetry/resource';
import { dashboardApi } from './functions/dashboard-api/resource';

const backend = defineBackend({
  auth,
  data,
  ingestTelemetry,
  dashboardApi,
});

// Custom resources live in their own stack (Amplify Gen2's documented escape
// hatch for anything outside auth/data/functions) rather than piggybacking on
// a generated resource's stack.
const customResources = backend.createStack('CloudBridgeResources');

// --- Telemetry table --------------------------------------------------------
// Raw CDK, not `a.model()`: Amplify Data's model-backed DynamoDB tables don't
// expose a way to turn on native attribute TTL, which the 90-day retention
// requirement needs, and this table is written/read only by the two Lambdas
// below (never directly by the GraphQL client) so modeling it wins nothing.
const telemetryTable = new Table(customResources, 'TelemetryTable', {
  partitionKey: { name: 'hubId', type: AttributeType.STRING },
  sortKey: { name: 'timestamp', type: AttributeType.STRING },
  billingMode: BillingMode.PAY_PER_REQUEST,
  timeToLiveAttribute: 'ttl',
});

const ingestLambda = backend.ingestTelemetry.resources.lambda;
const dashboardLambda = backend.dashboardApi.resources.lambda;

telemetryTable.grantWriteData(ingestLambda);
telemetryTable.grantReadData(dashboardLambda);

ingestLambda.addEnvironment('TELEMETRY_TABLE_NAME', telemetryTable.tableName);
dashboardLambda.addEnvironment('TELEMETRY_TABLE_NAME', telemetryTable.tableName);

// DescribeEndpoint has no resource-level scoping (account-wide by design);
// UpdateThingShadow/GetThingShadow are scoped to "thing/*" rather than "*" —
// still broad because a single hub is provisioned today, but this Lambda is
// the trusted, Cognito-gated backend, not a field device, so thing-level scope
// (vs. the much tighter per-topic device policy below) is the right line.
dashboardLambda.addToRolePolicy(
  new PolicyStatement({
    effect: Effect.ALLOW,
    actions: ['iot:DescribeEndpoint'],
    resources: ['*'],
  })
);
dashboardLambda.addToRolePolicy(
  new PolicyStatement({
    effect: Effect.ALLOW,
    actions: ['iot:UpdateThingShadow', 'iot:GetThingShadow'],
    resources: [`arn:aws:iot:${customResources.region}:${customResources.account}:thing/*`],
  })
);

// --- IoT: hub connection policy ---------------------------------------------
// One policy, attached to every hub's device certificate, using the
// ${iot:Connection.Thing.ThingName} policy variable so each connecting hub is
// confined to only its own farms/{hubId}/* and shadow topics — least
// privilege without a per-hub policy to author and maintain, matching the
// "single hub today, multi-hub-ready schema" design.
new CfnPolicy(customResources, 'HubIotPolicy', {
  policyName: 'smartfarm-hub-policy',
  policyDocument: JSON.stringify({
    Version: '2012-10-17',
    Statement: [
      {
        Effect: 'Allow',
        Action: 'iot:Connect',
        Resource: 'arn:aws:iot:*:*:client/${iot:Connection.Thing.ThingName}',
      },
      {
        Effect: 'Allow',
        Action: 'iot:Publish',
        Resource: 'arn:aws:iot:*:*:topic/farms/${iot:Connection.Thing.ThingName}/*',
      },
      {
        Effect: 'Allow',
        Action: 'iot:Subscribe',
        Resource: 'arn:aws:iot:*:*:topicfilter/$aws/things/${iot:Connection.Thing.ThingName}/shadow/*',
      },
      {
        Effect: 'Allow',
        Action: 'iot:Receive',
        Resource: 'arn:aws:iot:*:*:topic/$aws/things/${iot:Connection.Thing.ThingName}/shadow/*',
      },
      {
        Effect: 'Allow',
        Action: ['iot:UpdateThingShadow', 'iot:GetThingShadow'],
        Resource: 'arn:aws:iot:*:*:topic/$aws/things/${iot:Connection.Thing.ThingName}/shadow/*',
      },
    ],
  }),
});

// --- IoT: telemetry ingestion rule ------------------------------------------
// `topic(2)` pulls the hub id out of `farms/{hubId}/telemetry` itself, so the
// hub's message body doesn't need to duplicate its own id, and the `+`
// wildcard already routes every current/future hub through one rule.
ingestLambda.addPermission('IotRuleInvoke', {
  principal: new ServicePrincipal('iot.amazonaws.com'),
  sourceArn: `arn:aws:iot:${customResources.region}:${customResources.account}:rule/*`,
});

new CfnTopicRule(customResources, 'TelemetryIngestRule', {
  ruleName: 'smartfarm_telemetry_ingest',
  topicRulePayload: {
    sql: "SELECT *, topic(2) AS hubId FROM 'farms/+/telemetry'",
    awsIotSqlVersion: '2016-03-23',
    ruleDisabled: false,
    actions: [
      {
        lambda: {
          functionArn: ingestLambda.functionArn,
        },
      },
    ],
  },
});

export default backend;
