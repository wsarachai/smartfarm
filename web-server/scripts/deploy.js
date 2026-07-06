#!/usr/bin/env node
// Deploy wrapper: computes build metadata from git on the host (where .git
// exists) and passes it to `docker compose` as env -> build args, so the client
// bundle's Settings > About panel shows a real build number, commit, and date.
// Cross-platform (Node, not a shell one-liner) so it works from PowerShell too.
//
// Usage:  npm run deploy            -> docker compose up --build -d
//         npm run deploy -- <args>  -> docker compose <args> (with build meta)
const { execSync, spawnSync } = require('node:child_process');

function git(cmd, fallback) {
  try {
    return execSync(cmd, { stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim() || fallback;
  } catch {
    return fallback;
  }
}

const env = {
  ...process.env,
  BUILD_NUMBER: process.env.BUILD_NUMBER || git('git rev-list --count HEAD', 'dev'),
  GIT_SHA: process.env.GIT_SHA || git('git rev-parse --short HEAD', 'unknown'),
  BUILD_DATE: process.env.BUILD_DATE || new Date().toISOString(),
};

const passthrough = process.argv.slice(2);
const composeArgs = passthrough.length ? passthrough : ['compose', 'up', '--build', '-d'];

console.log(
  `[deploy] BUILD_NUMBER=${env.BUILD_NUMBER} GIT_SHA=${env.GIT_SHA} BUILD_DATE=${env.BUILD_DATE}`,
);

const result = spawnSync('docker', composeArgs, { stdio: 'inherit', env });
if (result.error) {
  console.error(`[deploy] failed to run docker: ${result.error.message}`);
  process.exit(1);
}
process.exit(result.status ?? 1);
