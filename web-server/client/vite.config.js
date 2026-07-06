import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { execSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

// --- Build info baked into the bundle at build time -----------------------
// Read at build time and injected via `define` as compile-time constants (see
// src/lib/buildInfo.js). Prefers git; falls back to env vars for environments
// without a .git (the Docker client-build stage, which receives them as build
// args -> ENV). Everything degrades gracefully so a bare build still succeeds.
function tryGit(cmd) {
  try {
    return execSync(cmd, { stdio: ['ignore', 'pipe', 'ignore'] })
      .toString()
      .trim();
  } catch {
    return '';
  }
}

const pkg = JSON.parse(
  readFileSync(fileURLToPath(new URL('./package.json', import.meta.url)), 'utf8'),
);

const version = pkg.version || '0.0.0';
const buildNumber = process.env.BUILD_NUMBER || tryGit('git rev-list --count HEAD') || 'dev';
const gitSha = process.env.GIT_SHA || tryGit('git rev-parse --short HEAD') || 'unknown';
const buildDate = process.env.BUILD_DATE || new Date().toISOString();

const buildInfo = { version, buildNumber, gitSha, buildDate };

export default defineConfig({
  plugins: [react()],
  define: {
    __BUILD_INFO__: JSON.stringify(buildInfo),
  },
  server: {
    proxy: {
      '/api': 'http://localhost:3000',
    },
  },
});
