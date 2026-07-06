# ---- Stage 1: build the React frontend into static assets ----
# Debian slim (glibc), not alpine/musl: avoids the npm "Exit handler never
# called!" crash seen building on ARM (Jetson Nano) with the alpine image.
FROM node:18-slim AS client-build
WORKDIR /app/client
COPY client/package*.json ./
# Jetson Nano links can be flaky; increase npm fetch retries/timeouts.
RUN npm config set fetch-retries 5 \
  && npm config set fetch-retry-mintimeout 20000 \
  && npm config set fetch-retry-maxtimeout 120000
RUN npm ci --no-audit --no-fund || npm install --include=dev --no-audit --no-fund
COPY client/ ./
RUN test -x node_modules/.bin/vite
# Build metadata baked into the bundle. The build context has no .git, so
# vite.config.js reads these from env (passed as build args by the deploy
# wrapper); a bare build omits them and falls back to dev/unknown + now().
ARG BUILD_NUMBER
ARG GIT_SHA
ARG BUILD_DATE
ENV BUILD_NUMBER=$BUILD_NUMBER \
  GIT_SHA=$GIT_SHA \
  BUILD_DATE=$BUILD_DATE
RUN npm run build

# ---- Stage 2: production runtime ----
FROM node:18-slim AS runtime
WORKDIR /app
ENV NODE_ENV=production
ENV PORT=3000

COPY package*.json ./
RUN npm config set fetch-retries 5 \
  && npm config set fetch-retry-mintimeout 20000 \
  && npm config set fetch-retry-maxtimeout 120000
RUN npm ci --omit=dev --no-audit --no-fund || npm install --omit=dev --no-audit --no-fund

COPY server.js ./
COPY src/ ./src/
COPY --from=client-build /app/client/dist ./client/dist

EXPOSE 3000
CMD ["node", "server.js"]
