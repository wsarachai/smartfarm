#!/usr/bin/env python3
"""
receiver.py — Raspberry Pi LoRa gateway for the Smart Farm.

Drives an SX1278 on the Pi's SPI0, receives the sensor node's 12-byte LoRa
frame, decodes it, maps node_id -> device_id, and POSTs {device_id, metrics} to
the web-server's telemetry endpoint. Standalone (like lora-gateway/bridge and the
AI poller) so the web-server stays hardware-agnostic — it needs ZERO changes.

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

# node_id -> friendly device_id (JSON in env, e.g. {"1":"water-temp-01"})
NODE_MAP = json.loads(_clean(os.environ.get("NODE_MAP", '{"1":"water-temp-01"}')))


def log(*args):
    print(datetime.now(timezone.utc).isoformat(), *args, flush=True)


def device_id(node_id):
    return NODE_MAP.get(str(node_id), "water-node-%d" % node_id)


def build_metrics(pkt, rssi, snr):
    m = {}
    if pkt["flags"] & lora_packet.FLAG_HOT and pkt["temp_hot_c100"] != lora_packet.TEMP_INVALID:
        m["temp_hot"] = round(pkt["temp_hot_c100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_COLD and pkt["temp_cold_c100"] != lora_packet.TEMP_INVALID:
        m["temp_cold"] = round(pkt["temp_cold_c100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_BATT and pkt["battery_mv"] > 0:
        m["battery_v"] = round(pkt["battery_mv"] / 1000.0, 3)
    # v2 BME280 fields
    if pkt["flags"] & lora_packet.FLAG_AIR:
        m["air_temp"] = round(pkt["air_temp_c100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_HUM:
        m["humidity"] = round(pkt["humidity_x100"] / 100.0, 2)
    if pkt["flags"] & lora_packet.FLAG_PRESS:
        m["pressure"] = round(pkt["pressure_dhpa"] / 10.0, 1)  # hPa
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
                post_telemetry(device_id(pkt["node_id"]), build_metrics(pkt, rssi, snr))
        time.sleep(0.05)


if __name__ == "__main__":
    raise SystemExit(main())
