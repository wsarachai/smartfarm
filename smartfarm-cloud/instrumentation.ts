// Next.js calls register() once when a new server instance starts — the
// supported hook for "start a long-lived background connection" in a
// self-hosted (next start) app, without resorting to a custom server.js.
// This is what actually replaces AWS IoT Core's always-on subscription: the
// MQTT client here lives for the lifetime of the Node process, not per-request.
export async function register() {
  // instrumentation.ts also runs in the edge runtime; the MQTT client and
  // Prisma are both Node-only, so only start them in the nodejs runtime.
  if (process.env.NEXT_RUNTIME === 'nodejs') {
    const { startMqtt } = await import('./lib/mqtt');
    startMqtt();
  }
}
