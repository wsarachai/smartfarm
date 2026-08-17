# AWS IoT Core Setup — Step by Step

Provisions one hub (Jetson/RPi) to talk to AWS IoT Core so `web-server`'s
`src/cloud/awsIotBridge.js` can connect. This is a one-time, per-hub, manual process —
it can't be scripted from a laptop because a physical private key has to end up on the
physical device. See [`aws-cloud-bridge.md`](aws-cloud-bridge.md) for what this
connects to and why; this doc is just the how.

Every command below uses the AWS CLI so it's exact and copy-pasteable. Console
equivalents exist for each step if you prefer clicking through the AWS IoT Core
console instead — the CLI is just less ambiguous to write down precisely.

## Prerequisites

- An AWS account (IoT Core + this feature's Lambda/DynamoDB usage is effectively free
  at single-farm scale, but an account with billing enabled is required).
- **The `smartfarm-cloud` backend deployed at least once.** The certificate policy you
  attach in Step 5 (`smartfarm-hub-policy`) is created by that project's Amplify Gen2
  backend, not by this doc — it has to exist before you can attach it.
- A name for this hub. Used consistently below as `jetson-01` — swap in your own
  (this becomes both the IoT **Thing name** and, by default, the `hubId` used
  throughout the telemetry/DynamoDB schema).

## Step 1 — Install & configure the AWS CLI

Skip if you already have it working.

```bash
# macOS
brew install awscli

# Linux
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip && sudo ./aws/install

# Windows: download and run the MSI at
# https://awscli.amazonaws.com/AWSCLIV2.msi
```

Then configure it with credentials for your account:

```bash
aws configure
# AWS Access Key ID: ...
# AWS Secret Access Key: ...
# Default region name: <the region you'll deploy smartfarm-cloud to, e.g. ap-southeast-1>
# Default output format: json
```

Verify:

```bash
aws sts get-caller-identity
```

## Step 2 — Deploy the `smartfarm-cloud` backend (if you haven't yet)

```bash
cd smartfarm-cloud
npm install
npx ampx sandbox
```

Wait for it to report the sandbox is deployed (this provisions Cognito, DynamoDB, the
Lambdas, **and** the custom IoT resources — the `smartfarm-hub-policy` policy and the
telemetry ingestion Topic Rule). You can `Ctrl-C` once it says deployed; the sandbox
doesn't need to stay running for the steps below.

Confirm the policy landed:

```bash
aws iot get-policy --policy-name smartfarm-hub-policy
```

If this errors with "policy not found," the sandbox deploy didn't finish — go back to
this step before continuing.

## Step 3 — Register the hub as an IoT Thing

```bash
aws iot create-thing --thing-name rasp-01
```

## Step 4 — Create the X.509 certificate and keys

```bash
mkdir -p ~/smartfarm-certs/rasp-01 && cd ~/smartfarm-certs/rasp-01

aws iot create-keys-and-certificate \
  --set-as-active \
  --certificate-pem-outfile certificate.pem.crt \
  --public-key-outfile public.pem.key \
  --private-key-outfile private.pem.key
```

This prints JSON including a `"certificateArn"` — **copy it**, you need it in the next
two steps. `private.pem.key` is generated only once; AWS never lets you retrieve it
again after this call, so don't lose the file.

## Step 5 — Attach the hub policy to the certificate

```bash
aws iot attach-policy \
  --policy-name smartfarm-hub-policy \
  --target "<certificateArn from Step 4>"
```

This is what confines this cert to only `farms/rasp-01/*` and its own shadow topics
(see `HubIotPolicy` in `smartfarm-cloud/amplify/backend.ts` — the policy is generic,
but each cert can only act as the Thing it's attached to, so `rasp-01`'s cert can
never touch `rasp-02`'s topics).

## Step 6 — Attach the certificate to the Thing

```bash
aws iot attach-thing-principal \
  --thing-name rasp-01 \
  --principal "<certificateArn from Step 4>"
```

## Step 7 — Download the Amazon Root CA

```bash
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
```

(Still in `~/smartfarm-certs/rasp-01/` — you should now have four files there:
`certificate.pem.crt`, `private.pem.key`, `public.pem.key` (unused by the bridge, keep
or discard), `AmazonRootCA1.pem`.)

## Step 8 — Get your account's IoT data-plane endpoint

```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

Copy the `endpointAddress` (looks like `xxxxxxxxxxxxx-ats.iot.<region>.amazonaws.com`)
— this is `AWS_IOT_ENDPOINT`.

## Step 9 — Copy the files to the hub and configure env vars

Copy `certificate.pem.crt`, `private.pem.key`, and `AmazonRootCA1.pem` onto the
Jetson/RPi, e.g. into `web-server/certs/` (already gitignored — never commit these).

Add to `web-server/.env` (copy from `.env.example` first if you haven't):

```env
AWS_IOT_ENDPOINT=xxxxxxxxxxxxx-ats.iot.<region>.amazonaws.com
AWS_IOT_CERT_PATH=./certs/certificate.pem.crt
AWS_IOT_KEY_PATH=./certs/private.pem.key
AWS_IOT_CA_PATH=./certs/AmazonRootCA1.pem
AWS_IOT_THING_NAME=rasp-01
```

`AWS_IOT_HUB_ID` is optional — it defaults to `AWS_IOT_THING_NAME`, which is correct
unless you specifically want the telemetry `hubId` to differ from the IoT Thing name.

## Step 10 — Restart `web-server` and verify the connection

For local testing, use `npm run dev`, **not** `npm start` — `start` runs plain `node
server.js` and never loads `.env`, so the bridge will report "not configured" even with
everything set up correctly. Only `dev` (`node --env-file=.env server.js`) loads it,
matching this project's existing convention for every other `.env`-configured value
(`PUMP_URL`, etc.). For a real Docker deployment, `docker-compose.yaml` needs these
same `AWS_IOT_*` vars passed through as container environment instead.

```bash
cd web-server
npm run dev       # local testing — loads .env
# or, for a real deployment: docker compose up -d --build (with AWS_IOT_* set in the container env)
```

Look for this in the logs:

```
[aws-iot] connected to xxxxxxxxxxxxx-ats.iot.<region>.amazonaws.com as "rasp-01"
```

If instead you see `not configured (AWS_IOT_* env vars missing/invalid)`, you're
probably running `npm start` instead of `npm run dev` — see above. If the env vars
*are* loading (double-check file paths and thing name in `.env`) but you only see
repeated `[aws-iot] reconnecting…` lines with **no** `connection error` message ever
printed, jump to Troubleshooting below — that specific silent-loop symptom has a single
near-always cause.

To confirm telemetry is actually arriving in AWS, open **AWS IoT Console → MQTT test
client → Subscribe to a topic** and subscribe to `farms/rasp-01/telemetry`, or from
the CLI:

```bash
aws iot-data get-thing-shadow --thing-name rasp-01 /dev/stdout
```

## Step 11 — Verify the command path end-to-end

Simulate what the cloud dashboard does — write desired state to the shadow:

```bash
aws iot-data update-thing-shadow \
  --thing-name rasp-01 \
  --cli-binary-format raw-in-base64-out \
  --payload '{"state":{"desired":{"main-pump":{"state":"on"}}}}' \
  /dev/stdout
```

Watch the hub's logs — you should see the pump command execute (or, if
`irrigation.auto` is on, a refusal). Then check what got reported back:

```bash
aws iot-data get-thing-shadow --thing-name rasp-01 /dev/stdout
```

`state.reported.main-pump` should show the outcome (`ok`, `relay_status`, `at`).

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `policy not found` in Step 2's check | `smartfarm-cloud` sandbox hasn't finished deploying — rerun `ampx sandbox` |
| `[aws-iot] not configured` at boot | You ran `npm start` instead of `npm run dev` (see Step 10) — otherwise, one of the five required env vars is missing, or a file path is wrong/unreadable |
| Repeated `[aws-iot] reconnecting…` with **no** `connection error` message ever printed | **Cert not attached to the Thing (Step 6 skipped or failed silently).** This is the single most common cause and is easy to confirm: `aws iot list-thing-principals --thing-name rasp-01` — an empty `principals: []` means exactly this. TLS always succeeds with any valid AWS-issued cert (so `openssl s_client -connect <endpoint>:8883 -cert ... -key ... -CAfile ...` reporting `Verification: OK` does **not** rule this out), but without a Thing attachment, `${iot:Connection.Thing.ThingName}` can't resolve, so `iot:Connect` is silently denied and AWS just drops the socket post-handshake — mqtt.js surfaces this as a bare reconnect loop, not an `error` event. Fix: `aws iot attach-thing-principal --thing-name rasp-01 --principal <certificateArn>` |
| Repeated `connection error` in logs **with a message** | Wrong `AWS_IOT_ENDPOINT` (region/typo), or the cert itself is INACTIVE/revoked — check `aws iot describe-certificate --certificate-id <id>` |
| TLS handshake fails specifically | Hub's system clock is significantly wrong — mutual TLS cares about validity windows; check `edge-ctrl`'s DS3231 RTC sync is running |
| Shadow updates accepted but hub never reacts | Confirm the delta actually reached `$aws/things/rasp-01/shadow/update/delta` — subscribe to it in the MQTT test client while running Step 11 |
| `AccessDeniedException` on any `aws iot`/`aws iot-data` CLI call above | Your CLI user's IAM permissions don't include IoT — this is separate from the hub's device policy; grant the CLI user `iot:*` or use an admin role for setup |

## Security notes

- `private.pem.key` must never be committed or shared — `certs/` is gitignored in
  `web-server/.gitignore` for exactly this reason.
- The `smartfarm-hub-policy` is intentionally scoped (see
  [`aws-cloud-bridge.md`](aws-cloud-bridge.md)) — don't broaden it to `Resource: "*"` for
  convenience; that would let one compromised hub cert act as every hub.
- If a hub is decommissioned or its key is suspected compromised, deactivate and delete
  its certificate rather than reusing it:
  ```bash
  aws iot update-certificate --certificate-id <id> --new-status INACTIVE
  aws iot detach-thing-principal --thing-name rasp-01 --principal <certificateArn>
  aws iot delete-certificate --certificate-id <id>
  ```
