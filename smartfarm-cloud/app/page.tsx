'use client';

import { useCallback, useEffect, useState } from 'react';
import { HUB_ID, PUMP_DEVICE_ID } from '../lib/hubConfig';

interface TelemetryRow {
  deviceId: string;
  timestamp: string;
  metrics: Record<string, unknown>;
}

interface LatestResponse {
  brokerConnected: boolean;
  readings: TelemetryRow[];
}

const LATEST_POLL_MS = 5000;

export default function DashboardPage() {
  const [latest, setLatest] = useState<LatestResponse | null>(null);
  const [history, setHistory] = useState<TelemetryRow[]>([]);
  const [loadingHistory, setLoadingHistory] = useState(false);
  const [pumpBusy, setPumpBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const loadHistory = useCallback(async () => {
    setLoadingHistory(true);
    setError(null);
    try {
      const res = await fetch(`/api/telemetry/history?hubId=${HUB_ID}&hours=24`);
      if (!res.ok) throw new Error((await res.json()).error ?? 'Failed to load telemetry');
      setHistory(await res.json());
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to load telemetry');
    } finally {
      setLoadingHistory(false);
    }
  }, []);

  useEffect(() => {
    loadHistory();
  }, [loadHistory]);

  // Lightweight polling for live values, matching the local web-server
  // dashboard's pattern — plain interval, cleaned up on unmount so it can't
  // leak across navigations.
  useEffect(() => {
    let cancelled = false;

    const poll = async () => {
      try {
        const res = await fetch(`/api/telemetry/latest?hubId=${HUB_ID}`);
        if (!res.ok || cancelled) return;
        setLatest(await res.json());
      } catch {
        // transient network hiccup — next tick retries, nothing to surface
      }
    };

    poll();
    const interval = setInterval(poll, LATEST_POLL_MS);
    return () => {
      cancelled = true;
      clearInterval(interval);
    };
  }, []);

  const sendPump = async (state: 'on' | 'off') => {
    setPumpBusy(true);
    setError(null);
    try {
      const res = await fetch('/api/pump/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hubId: HUB_ID, deviceId: PUMP_DEVICE_ID, state }),
      });
      if (!res.ok) throw new Error((await res.json()).error ?? 'Failed to send pump command');
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to send pump command');
    } finally {
      setPumpBusy(false);
    }
  };

  return (
    <main style={{ padding: '2rem', maxWidth: 900, margin: '0 auto' }}>
      <h1>SmartFarm Cloud</h1>
      <p style={{ color: '#666' }}>
        Hub: {HUB_ID} · Broker: {latest?.brokerConnected ? 'connected' : 'disconnected'}
      </p>

      <section>
        <h2>Pump control</h2>
        <button onClick={() => sendPump('on')} disabled={pumpBusy}>
          Turn ON
        </button>
        <button onClick={() => sendPump('off')} disabled={pumpBusy}>
          Turn OFF
        </button>
        <p style={{ fontSize: '0.85rem', color: '#666' }}>
          Publishes to the hub&apos;s command topic; the hub applies it (and runs it through its
          usual safety/auto-off logic) only if it&apos;s online right now — there&apos;s no
          offline queueing.
        </p>
      </section>

      <section>
        <h2>Latest readings</h2>
        <table>
          <thead>
            <tr>
              <th>Device</th>
              <th>Last seen</th>
              <th>Metrics</th>
            </tr>
          </thead>
          <tbody>
            {(latest?.readings.length ?? 0) === 0 && (
              <tr>
                <td colSpan={3}>No live readings yet.</td>
              </tr>
            )}
            {latest?.readings.map((row) => (
              <tr key={row.deviceId}>
                <td>{row.deviceId}</td>
                <td>{row.timestamp}</td>
                <td>{JSON.stringify(row.metrics)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>

      <section>
        <h2>Recent telemetry (24h)</h2>
        <button onClick={loadHistory} disabled={loadingHistory}>
          {loadingHistory ? 'Loading…' : 'Refresh'}
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
            {history.length === 0 && !loadingHistory && (
              <tr>
                <td colSpan={3}>No telemetry in the last 24h.</td>
              </tr>
            )}
            {history.map((row, i) => (
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
