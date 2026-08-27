#!/usr/bin/env python3
"""
receiver.py — Raspberry Pi LoRa gateway for the Smart Farm.

Drives an SX1278 on the Pi's SPI0, receives the sensor node's LoRa frame (v1, v2
or v3 — see lora_packet.py), decodes it, maps node_id -> device_id, and POSTs
{device_id, metrics} to the web-server's telemetry endpoint. Standalone (like
lora-gateway/bridge and the AI poller) so the web-server stays hardware-agnostic
— it needs ZERO changes.

  node (SX1278 433) --LoRa--> Pi SX1278 (SPI0) --> this --HTTP--> web-server:3000

Config via env (see .env.example). Stdlib-only HTTP (urllib); the only pip deps
are spidev + gpiozero (+ lgpio on Bookworm). Run as a systemd service.
"""
import json
import os
import time
import urllib.request
from datetime import datetime, timezone

from sx127x import SX127x
import lora_packet

# ---- config (env with defaults; both ends must share the LoRa PHY) ----------
def _clean(v):
    # Tolerate a trailing inline "# comment" + whitespace. systemd's
    # EnvironmentFile (unlike a shell) keeps inline comments in the value, so a
    # `.env` line like `LORA_BW_CODE=7  # 125 kHz` would otherwise crash int().
    return v.split("#", 1)[0].strip()


def env_int(name, default, base=10):
    return int(_clean(os.environ.get(name, default)), base)


SERVER_URL = _clean(os.environ.get("SERVER_URL", "http://localhost:3000/api/v1/telemetry"))
LORA_FREQ_HZ = env_int("LORA_FREQ_HZ", "433000000")
LORA_SF = env_int("LORA_SF", "9")
LORA_BW_CODE = env_int("LORA_BW_CODE", "7")        # 7 = 125 kHz
LORA_CR_CODE = env_int("LORA_CR_CODE", "1")        # 1 = 4/5
LORA_PREAMBLE = env_int("LORA_PREAMBLE", "8")
LORA_SYNCWORD = env_int("LORA_SYNCWORD", "0x12", 0)
SPI_BUS = env_int("SPI_BUS", "0")
SPI_DEV = env_int("SPI_DEV", "0")
RESET_PIN = env_int("RESET_PIN", "22")
DIO0_PIN = env_int("DIO0_PIN", "25")

# Site elevation (m) for reducing BME280 station pressure to mean sea level.
# 0 = disabled (report station pressure only).
ALTITUDE_M = float(_clean(os.environ.get("ALTITUDE_M", "0")))

# node_id -> friendly device_id (JSON in env, e.g. {"1":"water-temp-01"})
NODE_MAP = json.loads(_clean(os.environ.get("NODE_MAP", '{"1":"water-temp-01"}')))


def log(*args):
    print(datetime.now(timezone.utc).isoformat(), *args, flush=True)


def device_id(node_id):
    return NODE_MAP.get(str(node_id), "water-node-%d" % node_id)


_last_shape = {}


def log_shape_change(pkt):
    """Log the frame version + temp/RH source once, and again only if it changes
    (a node swap, a firmware update, or an SHT45 that stopped answering)."""
    shape = (pkt["version"], bool(pkt["flags"] & lora_packet.FLAG_SHT))
    if _last_shape.get(pkt["node_id"]) == shape:
        return
    _last_shape[pkt["node_id"]] = shape
    log("# node %d: frame v%d, air temp/humidity from %s"
        % (pkt["node_id"], shape[0], "SHT45" if shape[1] else "BME280"))


def pressure_to_msl(p_station_hpa, altitude_m, temp_c):
    """Reduce station pressure to mean sea level (temperature-corrected formula,
    the standard 'reduction to sea level' used by weather stations)."""
    return p_station_hpa * (1.0 - (0.0065 * altitude_m) /
                            (temp_c + 0.0065 * altitude_m + 273.15)) ** -5.257


def build_metrics(pkt, rssi, snr):
    m = {}
    if pkt["flags"] & lora_packet.FLAG_HOT and pkt["temp_hot_c100"] != lora_packet.TEMP_INVALID:
        m["temp_hot"] = round(pkt["temp_hot_c100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_COLD and pkt["temp_cold_c100"] != lora_packet.TEMP_INVALID:
        m["temp_cold"] = round(pkt["temp_cold_c100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_BATT and pkt["battery_mv"] > 0:
        m["battery_v"] = round(pkt["battery_mv"] / 1000.0, 3)
    # v2+ ambient fields. air_temp/humidity come from the SHT45 when FLAG_SHT is
    # set (v3 nodes) and from the BME280 otherwise — same units either way, so
    # the metric names do not change and dashboard history stays continuous.
    if pkt["flags"] & lora_packet.FLAG_AIR:
        m["air_temp"] = round(pkt["air_temp_c100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_HUM:
        m["humidity"] = round(pkt["humidity_x100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_PRESS:
        station_hpa = pkt["pressure_dhpa"] / 10.0
        m["pressure"] = round(station_hpa, 1)  # raw station pressure, hPa
        if ALTITUDE_M > 0:
            # use measured air temp if present, else the 15 C standard atmosphere
            temp_c = (pkt["air_temp_c100"] / 100.0
                      if pkt["flags"] & lora_packet.FLAG_AIR else 15.0)
            m["pressure_msl"] = round(pressure_to_msl(station_hpa, ALTITUDE_M, temp_c), 1)
    # v3 SCD41 field. Integer ppm — sub-ppm resolution would be noise.
    if pkt["flags"] & lora_packet.FLAG_CO2 and pkt["co2_ppm"] > 0:
        m["co2"] = pkt["co2_ppm"]
    m["rssi"] = rssi
    m["snr"] = round(snr, 1)
    m["seq"] = pkt["seq"]
    return m


def post_telemetry(device, metrics):
    body = json.dumps({"device_id": device, "metrics": metrics}).encode()
    req = urllib.request.Request(
        SERVER_URL, data=body,
        headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            if resp.status // 100 != 2:
                log("POST %d for %s" % (resp.status, device))
            else:
                log("->", device, json.dumps(metrics))
    except Exception as e:
        log("POST failed:", e)


def main():
    radio = SX127x(SPI_BUS, SPI_DEV, RESET_PIN, DIO0_PIN, LORA_FREQ_HZ)
    ok = radio.begin()
    log("SX127x RegVersion=0x%02X (expect 0x12) -> %s"
        % (radio.version(), "OK" if ok else "FAULT"))
    if not ok:
        log("SPI/wiring FAULT: check SPI enabled (raspi-config) + NSS/SCK/MISO/MOSI/RESET wiring")
        radio.close()
        return 1

    radio.config_lora(LORA_SF, LORA_BW_CODE, LORA_CR_CODE, LORA_PREAMBLE,
                      LORA_SYNCWORD, crc_on=True)
    radio.rx_start()
    log("listening @ %.3f MHz SF%d BW-code %d CR-code %d sync 0x%02X -> %s"
        % (LORA_FREQ_HZ / 1e6, LORA_SF, LORA_BW_CODE, LORA_CR_CODE, LORA_SYNCWORD, SERVER_URL))

    while True:
        res = radio.rx_poll()
        if res == "crc":
            log("rx crc error (dropped)")
        elif res is not None:
            payload, rssi, snr = res
            pkt = lora_packet.decode(payload)
            if pkt is None:
                log("rx bad frame (magic/crc/len), %dB rssi=%d" % (len(payload), rssi))
            else:
                log_shape_change(pkt)
                post_telemetry(device_id(pkt["node_id"]), build_metrics(pkt, rssi, snr))
        time.sleep(0.05)


if __name__ == "__main__":
    raise SystemExit(main())
