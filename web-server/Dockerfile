# ---- Stage 1: build the React frontend into static assets ----
FROM node:18-alpine AS client-build
WORKDIR /app/client
COPY client/package*.json ./
RUN npm ci
COPY client/ ./
RUN npm run build

# ---- Stage 2: production runtime ----
FROM node:18-alpine AS runtime
WORKDIR /app
ENV NODE_ENV=production
ENV PORT=3000

COPY package*.json ./
RUN npm ci --omit=dev

COPY server.js ./
COPY src/ ./src/
COPY --from=client-build /app/client/dist ./client/dist

EXPOSE 3000
CMD ["node", "server.js"]
