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
├── lib/hubConfig.ts            # single hub id, kept out of hardcoded paths
└── amplify.yml                 # Amplify Hosting buildspec (backend+frontend)
```

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

## Not implemented here (needs live AWS credentials to go further)

- Nothing has been run against real AWS — `ampx sandbox` was never executed
  in this environment, so `amplify_outputs.json` doesn't exist and the
  project has never been `npm install`ed, type-checked, or built.
- The one-time physical provisioning steps (Thing registration, cert
  generation, attaching the policy, copying certs to the hub) are manual by
  design and unactionable without console/CLI access to a real account.
- Amplify Hosting itself (git-connect + domain) is intentionally not
  configured — `amplify.yml` is written to be ready for it, but connecting
  the app is an account-level action.
