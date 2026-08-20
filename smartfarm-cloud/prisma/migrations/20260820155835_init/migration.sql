-- CreateTable
CREATE TABLE "telemetry_readings" (
    "id" BIGSERIAL NOT NULL,
    "hubId" TEXT NOT NULL,
    "deviceId" TEXT NOT NULL,
    "timestamp" TIMESTAMP(3) NOT NULL,
    "metrics" JSONB NOT NULL,

    CONSTRAINT "telemetry_readings_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE INDEX "telemetry_readings_hubId_timestamp_idx" ON "telemetry_readings"("hubId", "timestamp");
