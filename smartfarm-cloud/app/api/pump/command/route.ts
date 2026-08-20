import { NextRequest, NextResponse } from 'next/server';
import { publishCommand } from '../../../../lib/mqtt';

// Replaces sendPumpCommand's UpdateThingShadowCommand write. Fire-and-forget
// by design (see docs/mqtt-cloud-bridge.md) — a 200 here means the command
// was published to the broker, not that the hub received or executed it. If
// the hub is offline the message is simply dropped; there's no shadow-style
// desired/reported diffing to redeliver it later.
export async function POST(request: NextRequest) {
  const body = await request.json().catch(() => null);
  const hubId = body?.hubId;
  const deviceId = body?.deviceId;
  const state = body?.state;

  if (!hubId || !deviceId || !state) {
    return NextResponse.json({ error: 'hubId, deviceId, and state are required' }, { status: 400 });
  }

  const result = publishCommand(hubId, deviceId, state);
  if (!result.ok) {
    return NextResponse.json({ error: result.error }, { status: 503 });
  }
  return NextResponse.json({ ok: true });
}
