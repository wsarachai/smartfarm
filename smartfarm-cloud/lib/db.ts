import { PrismaClient } from '@prisma/client';

// Standard Next.js dev-mode singleton: without this, every hot-reload of a
// module that imports `db` would open a new PrismaClient (and a new
// connection pool) against the same process, quickly exhausting Postgres's
// max_connections. Not an issue in production (`next start` doesn't hot
// reload), but harmless to keep either way.
const globalForPrisma = globalThis as unknown as { prisma?: PrismaClient };

export const db = globalForPrisma.prisma ?? new PrismaClient();

if (process.env.NODE_ENV !== 'production') {
  globalForPrisma.prisma = db;
}
