/** @type {import('next').NextConfig} */
const nextConfig = {
  // Needed on Next 14 for instrumentation.ts's register() hook (starts the
  // MQTT client) to run — stable/default-on in Next 15+, but this repo is
  // pinned to ^14.2.5.
  experimental: {
    instrumentationHook: true,
  },
};

export default nextConfig;
