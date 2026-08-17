# smartfarm-cloud — remote monitoring & control

The cloud half of the AWS bridge (see `../docs` on the hub side / the plan
this was built from). A Next.js dashboard on **AWS Amplify Hosting**, backed
by an **Amplify Gen2** backend: Cognito auth, a DynamoDB telemetry table,
and two Lambdas wired to AWS IoT Core. No always-on server — everything here
is Lambda/Amplify-managed.

```
smartfarm-cloud/
├── amplify/
│   ├── backend.ts              # defineBackend + raw CDK escape hatches
│   │                           #   (telemetry table, IoT policy, IoT rule)
│   ├── auth/resource.ts        # Cognito user pool (email/password)
│   ├── data/resource.ts        # custom query/mutation schema (no a.model())
│   └── functions/
│       ├── ingest-telemetry/   # IoT Rule target: writes telemetry → DynamoDB
│       └── dashboard-api/      # AppSync Lambda resolver: history read +
│                                #   pump command write (Device Shadow)
├── app/                        # Next.js App Router, auth-gated at the root
│   ├── layout.tsx
│   ├── providers.tsx           # Amplify.configure + <Authenticator> wrapper
│   └── page.tsx                # telemetry table + pump on/off
└── lib/hubConfig.ts            # single hub id, kept out of hardcoded paths
```

Amplify Hosting's build spec lives at the **repo root** (`../amplify.yml`), not here —
Hosting is configured as a monorepo app (app root = `smartfarm-cloud`), and in that mode
it reads a repo-root spec in the `applications:`-wrapped monorepo format, not a
single-app spec inside the app's own directory.

## Why a raw CDK table instead of `a.model()`

Amplify Data's model-backed DynamoDB tables don't expose a way to enable
native attribute TTL, and the 90-day retention requirement needs exactly
that. So `TelemetryTable` is a plain `aws-cdk-lib/aws-dynamodb.Table`
construct added directly in `backend.ts` (partition key `hubId`, sort key
`timestamp`, `timeToLiveAttribute: 'ttl'`), per the plan's documented
fallback. It's written only by `ingest-telemetry` and read only by
`dashboard-api`, never touched directly by the GraphQL client, so nothing is
lost by not modeling it as an `a.model()` type.

## Why a custom query/mutation instead of hand-wired API Gateway

The dashboard's read (`getTelemetryHistory`) and write (`sendPumpCommand`)
paths are exposed as **custom Amplify Data operations** (`a.query()` /
`a.mutation()` backed by `a.handler.function(dashboardApi)`) rather than a
separate API Gateway + Cognito authorizer stack. This reuses the AppSync API
Amplify Data already provisions — Cognito user-pool authorization
(`allow.authenticated()`) and a typed `generateClient<Schema>()` for the
frontend come for free, with no hand-rolled REST auth wiring. `dashboard-api`
is one Lambda handling both operations, switched on `event.info.fieldName`.

## Setup (needs a live AWS account — not run here)

```
npm install
npx ampx sandbox        # provisions a personal dev backend, writes amplify_outputs.json
npm run dev
```

Then, per the plan's manual provisioning step: register the hub as an IoT
Thing, generate its X.509 cert, attach the `smartfarm-hub-policy` this
project creates (scoped to that thing's own `farms/{hubId}/*` topics via the
`${iot:Connection.Thing.ThingName}` policy variable — one policy covers every
current/future hub), and copy the cert/key/root CA onto the hub for
`web-server/src/cloud/awsIotBridge.js`.

## Status

Deployed and verified end-to-end against real AWS: `ampx sandbox` (identifier `keng`)
is live, telemetry ingestion and the pump command path (Device Shadow → hub →
`pumpControl.command()` → reported outcome) have both been confirmed working with a
real hub (`rasp-01`). See [`../docs/aws-cloud-bridge.md`](../docs/aws-cloud-bridge.md)
and [`../docs/aws-iot-setup.md`](../docs/aws-iot-setup.md) for the debugging history —
several real bugs were found and fixed this way (a Device Shadow payload-shape
mismatch, an IoT policy missing `iot:Publish` on `shadow/update`, a nested-stack
circular dependency, and this file's monorepo build-spec format).

- The one-time physical provisioning steps (Thing registration, cert generation,
  attaching the policy, copying certs to the hub) remain manual by design — see
  `docs/aws-iot-setup.md`.
- Amplify Hosting (git-connect) is in progress — connecting via the Console creates a
  **separate `main`-branch backend environment** (`ampx pipeline-deploy`), distinct from
  the `keng` sandbox. IoT policy/rule names are suffixed per-environment
  (`smartfarm-hub-policy-<stack-suffix>`) specifically so both can coexist without
  colliding.
- A custom domain isn't configured yet.
