# ---- Stage 1: build the React frontend into static assets ----
# Debian slim (glibc), not alpine/musl: avoids the npm "Exit handler never
# called!" crash seen building on ARM (Jetson Nano) with the alpine image.
FROM node:18-slim AS client-build
WORKDIR /app/client
COPY client/package*.json ./
# npm ci is deterministic (installs from package-lock.json) and fails loudly on
# error instead of leaving a half-installed tree that later breaks `vite build`.
RUN npm ci --no-audit --no-fund
COPY client/ ./
RUN npm run build

# ---- Stage 2: production runtime ----
FROM node:18-slim AS runtime
WORKDIR /app
ENV NODE_ENV=production
ENV PORT=3000

COPY package*.json ./
RUN npm ci --omit=dev --no-audit --no-fund

COPY server.js ./
COPY src/ ./src/
COPY --from=client-build /app/client/dist ./client/dist

EXPOSE 3000
CMD ["node", "server.js"]
