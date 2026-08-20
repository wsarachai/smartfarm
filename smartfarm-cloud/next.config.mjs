/** @type {import('next').NextConfig} */
const nextConfig = {
  // Needed on Next 14 for instrumentation.ts's register() hook (starts the
  // MQTT client) to run — stable/default-on in Next 15+, but this repo is
  // pinned to ^14.2.5.
  experimental: {
    instrumentationHook: true,
    // Without this, webpack bundles `mqtt` (and its `ws` transport
    // dependency) into the server chunk, which breaks ws's frame-masking
    // implementation ("TypeError: t.mask is not a function") — surfaced as
    // a recurring keepalive-timeout/reconnect loop on itsci-data.local,
    // roughly every ~15-30 min whenever a PING frame hit the broken path.
    // This tells Next to load `mqtt` via native require() at runtime
    // instead of bundling it.
    serverComponentsExternalPackages: ['mqtt'],
  },
};

export default nextConfig;
