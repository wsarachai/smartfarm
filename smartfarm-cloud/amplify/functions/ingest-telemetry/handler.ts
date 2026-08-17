import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, PutCommand } from '@aws-sdk/lib-dynamodb';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const TTL_SECONDS = 90 * 24 * 60 * 60;

// Shape delivered by the IoT Topic Rule (see backend.ts): the rule's SQL
// selects the raw telemetry payload from `farms/{hubId}/telemetry` and
// projects the hub id out of the topic itself via topic(2), since the hub
// doesn't (and shouldn't have to) embed its own id in the message body.
interface TelemetryRuleEvent {
  hubId: string;
  device_id: string;
  timestamp?: string;
  metrics?: Record<string, number>;
}

export const handler = async (event: TelemetryRuleEvent): Promise<void> => {
  const timestamp = event.timestamp ?? new Date().toISOString();
  const nowSeconds = Math.floor(Date.now() / 1000);

  await ddb.send(
    new PutCommand({
      TableName: process.env.TELEMETRY_TABLE_NAME,
      Item: {
        hubId: event.hubId,
        timestamp,
        deviceId: event.device_id,
        metrics: event.metrics ?? {},
        ttl: nowSeconds + TTL_SECONDS,
      },
    })
  );
};
