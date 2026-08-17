import { defineBackend } from '@aws-amplify/backend';
import { AttributeType, BillingMode, Table } from 'aws-cdk-lib/aws-dynamodb';
import { CfnPolicy, CfnTopicRule } from 'aws-cdk-lib/aws-iot';
import { Effect, PolicyStatement, ServicePrincipal } from 'aws-cdk-lib/aws-iam';
import type { Function as CdkFunction } from 'aws-cdk-lib/aws-lambda';
// Explicit .ts extensions: ampx's CDK Assembly step runs these files through
// Node's own native TypeScript loader, which — unlike tsx/esbuild/Next's
// bundler — does not resolve extensionless relative specifiers.
import { auth } from './auth/resource.ts';
import { data } from './data/resource.ts';
import { ingestTelemetry } from './functions/ingest-telemetry/resource.ts';
import { dashboardApi } from './functions/dashboard-api/resource.ts';

const backend = defineBackend({
  auth,
  data,
  ingestTelemetry,
  dashboardApi,
});

// Custom resources go directly in the root stack, NOT a new nested stack
// (backend.createStack(...) — the originally-tried approach): the table's
// consumers (ingestTelemetry, dashboardApi) live in Amplify's own nested
// "function" stack, and dashboardApi is also referenced by the nested "data"
// stack. A second custom nested stack that both reads their Lambda ARNs
// (for the IoT Topic Rule target and IAM grants) AND is read BY them (env
// vars for the table name) is a dependency cycle between SIBLING nested
// stacks, which CloudFormation can't order
// ([CloudformationStackCircularDependencyError] between CloudBridgeResources,
// data, and function — hit on an earlier attempt). The root stack is the
// common PARENT of all of them, and parent↔child nested-stack references are
// normal CloudFormation parameter/output passing, not a sibling cycle — so
// resources placed here can be freely referenced by every nested stack in
// either direction without reintroducing the problem.
const customResources = backend.stack;

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

// Amplify Gen2 narrows resources.lambda to the CDK IFunction interface (no
// addEnvironment), even though the underlying construct is a concrete
// NodejsFunction — cast to the concrete type to reach it, same as Amplify's
// own documented pattern for this exact case.
const ingestLambda = backend.ingestTelemetry.resources.lambda as CdkFunction;
const dashboardLambda = backend.dashboardApi.resources.lambda as CdkFunction;

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
  // CfnPolicy.policyDocument wants a plain object (CDK serializes it into the
  // CloudFormation template itself) — a pre-stringified JSON string fails the
  // L1 construct's runtime prop validation.
  policyDocument: {
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
      // MQTT publish/subscribe authorization uses iot:Publish/iot:Subscribe/
      // iot:Receive on topic ARNs — NOT iot:UpdateThingShadow/iot:GetThingShadow,
      // which are the *Data Plane REST/HTTPS API* actions (what smartfarm-cloud's
      // dashboard-api Lambda uses via UpdateThingShadowCommand, correctly, since
      // it calls the HTTPS API — see its addToRolePolicy grants). A prior version
      // of this policy granted those two actions here instead of iot:Publish; since
      // they don't apply to MQTT topic ARNs at all, that statement was a no-op —
      // the hub could receive shadow deltas but was never actually authorized to
      // publish its "reported" outcome back. AWS IoT Core disconnects a client
      // outright on any unauthorized action (not just silently drops the message),
      // and since a failed report leaves desired≠reported, the same delta gets
      // redelivered on every resubscribe-after-reconnect — producing a permanent,
      // fast reconnect loop the moment any command was ever sent. Found by live
      // debugging: connected cleanly, then looped immediately after the first
      // Device Shadow command was sent, never recovering.
      {
        Effect: 'Allow',
        Action: 'iot:Publish',
        Resource: 'arn:aws:iot:*:*:topic/$aws/things/${iot:Connection.Thing.ThingName}/shadow/update',
      },
    ],
  },
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
