import { NextRequest, NextResponse } from 'next/server';
import { getLatestReadings, isMqttConnected } from '../../../../lib/mqtt';

// Polled by the dashboard (matching the local web-server dashboard's
// polling-over-push pattern) for live per-device values. Backed by the
// in-memory cache lib/mqtt.ts fills as telemetry arrives — no DB round trip
// on every poll.
export async function GET(request: NextRequest) {
  const { searchParams } = new URL(request.url);
  const hubId = searchParams.get('hubId');

  if (!hubId) {
    return NextResponse.json({ error: 'hubId is required' }, { status: 400 });
  }

  return NextResponse.json({
    // Whether THIS server has a live connection to the broker — not whether
    // the hub itself is reachable (we have no direct signal for that beyond
    // reading recency, which the client can judge from each reading's own
    // timestamp).
    brokerConnected: isMqttConnected(),
    readings: getLatestReadings(hubId),
  });
}
