import type { Metadata } from 'next';
import type { ReactNode } from 'react';
import './globals.css';

export const metadata: Metadata = {
  title: 'SmartFarm Cloud',
  description: 'Remote monitoring and control for SmartFarm hubs',
};

// No auth wrapper here — this app is gated entirely at the edge by
// Cloudflare Access on the smartfarm.sarachai.com hostname, not in-app
// (see docs/mqtt-cloud-bridge.md). A request only ever reaches this process
// after Access has already authenticated it.
export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
