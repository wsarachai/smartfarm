import { NextRequest, NextResponse } from 'next/server';
import { db } from '../../../../lib/db';

// Replaces the AppSync getTelemetryHistory query / ingest-telemetry's
// DynamoDB-backed read. No auth check here — Cloudflare Access gates the
// whole app at the edge before any request reaches this route.
export async function GET(request: NextRequest) {
  const { searchParams } = new URL(request.url);
  const hubId = searchParams.get('hubId');
  const hours = Number(searchParams.get('hours') ?? '24');

  if (!hubId) {
    return NextResponse.json({ error: 'hubId is required' }, { status: 400 });
  }

  const since = new Date(Date.now() - hours * 60 * 60 * 1000);

  const rows = await db.telemetryReading.findMany({
    where: { hubId, timestamp: { gte: since } },
    orderBy: { timestamp: 'desc' },
    take: 500,
  });

  return NextResponse.json(
    rows.map((r) => ({
      deviceId: r.deviceId,
      timestamp: r.timestamp.toISOString(),
      metrics: r.metrics,
    }))
  );
}
