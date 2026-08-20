# Multi-stage build for the self-hosted dashboard (replaces Amplify Hosting's
# managed build). Runtime host is a normal x86_64 Linux box (itsci-data.local),
# not the resource-constrained Jetson/Pi the web-server hub targets, so this
# doesn't need the same aggressive image-size discipline — plain node:20-slim
# throughout is fine.

FROM node:20-slim AS build
WORKDIR /app
COPY package.json package-lock.json* ./
RUN npm install
COPY . .
# `npm run build` already runs `prisma generate` first (see package.json).
RUN npm run build

FROM node:20-slim AS runtime
WORKDIR /app
ENV NODE_ENV=production
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/.next ./.next
COPY --from=build /app/prisma ./prisma
COPY --from=build /app/package.json ./package.json
COPY --from=build /app/next.config.mjs ./next.config.mjs

EXPOSE 3000
# Applies any pending migrations before the app starts serving — safe to run
# on every container start (no-op if already up to date).
CMD ["sh", "-c", "npx prisma migrate deploy && npm start"]
