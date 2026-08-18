import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, QueryCommand } from '@aws-sdk/lib-dynamodb';
import { IoTClient, DescribeEndpointCommand } from '@aws-sdk/client-iot';
import { IoTDataPlaneClient, UpdateThingShadowCommand } from '@aws-sdk/client-iot-data-plane';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const iotControl = new IoTClient({});

// The account's IoT data-plane endpoint isn't a fixed/predictable value (it's
// account+region specific and only obtainable via DescribeEndpoint), so it's
// resolved lazily on first use and cached for the life of the execution
// environment rather than baked in at deploy time.
let iotDataClient: IoTDataPlaneClient | undefined;
async function getIotDataClient(): Promise<IoTDataPlaneClient> {
  if (iotDataClient) return iotDataClient;
  const { endpointAddress } = await iotControl.send(
    new DescribeEndpointCommand({ endpointType: 'iot:Data-ATS' })
  );
  iotDataClient = new IoTDataPlaneClient({ endpoint: `https://${endpointAddress}` });
  return iotDataClient;
}

interface GetHistoryArgs {
  hubId: string;
  hours?: number;
}

interface SendCommandArgs {
  hubId: string;
  deviceId: string;
  state: string;
}

type Args = GetHistoryArgs & Partial<SendCommandArgs>;

// NOT AppSyncResolverHandler from @types/aws-lambda — that type models
// AppSync's direct Lambda resolver invocation ({info: {fieldName, ...}}).
// Amplify Gen2's a.handler.function() instead wires this Lambda in as a
// pipeline function (see the "Invoke" data source's request mapping
// template), which sends typeName/fieldName/arguments/etc. as TOP-LEVEL
// event keys, not nested under `info`. event.info.fieldName is undefined
// on every real invocation with the direct-resolver type — confirmed via
// CloudWatch logs showing that exact TypeError, and via
// `aws appsync get-function` on the deployed pipeline function showing the
// real payload shape, before writing this fix.
interface PipelineFunctionEvent<A> {
  typeName: string;
  fieldName: string;
  arguments: A;
}

export const handler = async (event: PipelineFunctionEvent<Args>) => {
  switch (event.fieldName) {
    case 'getTelemetryHistory':
      return getTelemetryHistory(event.arguments as GetHistoryArgs);
    case 'sendPumpCommand':
      return sendPumpCommand(event.arguments as SendCommandArgs);
    default:
      throw new Error(`Unsupported field: ${event.fieldName}`);
  }
};

async function getTelemetryHistory({ hubId, hours = 24 }: GetHistoryArgs) {
  const since = new Date(Date.now() - hours * 60 * 60 * 1000).toISOString();
  const result = await ddb.send(
    new QueryCommand({
      TableName: process.env.TELEMETRY_TABLE_NAME,
      KeyConditionExpression: 'hubId = :hubId AND #ts >= :since',
      ExpressionAttributeNames: { '#ts': 'timestamp' },
      ExpressionAttributeValues: { ':hubId': hubId, ':since': since },
      ScanIndexForward: false,
      Limit: 500,
    })
  );
  return result.Items ?? [];
}

// Writes "desired" state to the hub's Device Shadow instead of calling the
// hub directly: the hub's persistent MQTT connection picks up the delta the
// moment it's next online, so a briefly-offline hub never drops a command or
// needs a hand-rolled retry queue here.
async function sendPumpCommand({ hubId, deviceId, state }: SendCommandArgs) {
  const client = await getIotDataClient();
  const payload = {
    state: {
      desired: {
        [deviceId]: { state },
      },
    },
  };
  await client.send(
    new UpdateThingShadowCommand({
      thingName: hubId,
      payload: new TextEncoder().encode(JSON.stringify(payload)),
    })
  );
  return 'ok';
}
