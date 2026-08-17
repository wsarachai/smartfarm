'use client';

import { useCallback, useEffect, useState } from 'react';
import { generateClient } from 'aws-amplify/data';
import { useAuthenticator } from '@aws-amplify/ui-react';
import type { Schema } from '../amplify/data/resource';
import { HUB_ID, PUMP_DEVICE_ID } from '../lib/hubConfig';

const client = generateClient<Schema>();

type TelemetryRow = {
  hubId?: string | null;
  deviceId?: string | null;
  timestamp?: string | null;
  metrics?: unknown;
};

export default function DashboardPage() {
  const { signOut, user } = useAuthenticator((ctx) => [ctx.user]);
  const [telemetry, setTelemetry] = useState<TelemetryRow[]>([]);
  const [loading, setLoading] = useState(false);
  const [pumpBusy, setPumpBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const loadHistory = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const { data: rows, errors } = await client.queries.getTelemetryHistory({
        hubId: HUB_ID,
        hours: 24,
      });
      if (errors?.length) throw new Error(errors[0].message);
      setTelemetry((rows ?? []).filter((r): r is TelemetryRow => r !== null));
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to load telemetry');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    loadHistory();
  }, [loadHistory]);

  const sendPump = async (state: 'on' | 'off') => {
    setPumpBusy(true);
    setError(null);
    try {
      const { errors } = await client.mutations.sendPumpCommand({
        hubId: HUB_ID,
        deviceId: PUMP_DEVICE_ID,
        state,
      });
      if (errors?.length) throw new Error(errors[0].message);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to send pump command');
    } finally {
      setPumpBusy(false);
    }
  };

  return (
    <main style={{ padding: '2rem', maxWidth: 900, margin: '0 auto' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' }}>
        <h1>SmartFarm Cloud</h1>
        <div>
          <span style={{ marginRight: '1rem', fontSize: '0.9rem' }}>{user?.signInDetails?.loginId}</span>
          <button onClick={signOut}>Sign out</button>
        </div>
      </div>
      <p style={{ color: '#666' }}>Hub: {HUB_ID}</p>

      <section>
        <h2>Pump control</h2>
        <button onClick={() => sendPump('on')} disabled={pumpBusy}>
          Turn ON
        </button>
        <button onClick={() => sendPump('off')} disabled={pumpBusy}>
          Turn OFF
        </button>
        <p style={{ fontSize: '0.85rem', color: '#666' }}>
          Writes the pump&apos;s desired state to the hub&apos;s Device Shadow; the hub applies it (and
          runs it through its usual safety/auto-off logic) the moment it&apos;s next online.
        </p>
      </section>

      <section>
        <h2>Recent telemetry (24h)</h2>
        <button onClick={loadHistory} disabled={loading}>
          {loading ? 'Loading…' : 'Refresh'}
        </button>
        {error && <p style={{ color: 'crimson' }}>{error}</p>}
        <table>
          <thead>
            <tr>
              <th>Timestamp</th>
              <th>Device</th>
              <th>Metrics</th>
            </tr>
          </thead>
          <tbody>
            {telemetry.length === 0 && !loading && (
              <tr>
                <td colSpan={3}>No telemetry in the last 24h.</td>
              </tr>
            )}
            {telemetry.map((row, i) => (
              <tr key={`${row.deviceId}-${row.timestamp}-${i}`}>
                <td>{row.timestamp}</td>
                <td>{row.deviceId}</td>
                <td>{JSON.stringify(row.metrics)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>
    </main>
  );
}
