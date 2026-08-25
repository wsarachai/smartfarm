/*
 * bridge.js — LoRa gateway serial -> HTTP bridge.
 *
 * Reads newline-delimited JSON lines from the NUCLEO-WL55JC gateway's USB
 * virtual COM port and POSTs each one to the Smart Farm web-server's telemetry
 * endpoint. Deliberately standalone (not inside web-server/) so the web-server
 * stays hardware-agnostic and dev-PC-safe — same reasoning as the AI poller.
 *
 * The gateway emits lines like:
 *   {"device_id":"water-temp-01","metrics":{"temp_hot":41.30,...,"rssi":-92,"snr":8.5}}
 * and diagnostic lines starting with '#', which we log and skip.
 *
 * Config via env (see .env.example):
 *   SERIAL_PORT   e.g. /dev/ttyACM0 (Linux/Jetson) or COM3 (Windows)
 *   SERIAL_BAUD   default 115200
 *   SERVER_URL    default http://localhost:3000/api/v1/telemetry
 *   RECONNECT_MS  serial reconnect backoff, default 3000
 *
 * Run: node bridge.js   (Node >= 18 for global fetch)
 */
'use strict';

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

const SERIAL_PORT = process.env.SERIAL_PORT || '/dev/ttyACM0';
const SERIAL_BAUD = parseInt(process.env.SERIAL_BAUD || '115200', 10);
const SERVER_URL = process.env.SERVER_URL || 'http://localhost:3000/api/v1/telemetry';
const RECONNECT_MS = parseInt(process.env.RECONNECT_MS || '3000', 10);

function log(...args) {
  console.log(new Date().toISOString(), ...args);
}

async function postTelemetry(obj) {
  const controller = new AbortController();
  const t = setTimeout(() => controller.abort(), 5000);
  try {
    const res = await fetch(SERVER_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(obj),
      signal: controller.signal,
    });
    if (!res.ok) {
      log(`POST ${res.status} for ${obj.device_id}`);
    } else {
      log(`-> ${obj.device_id}`, JSON.stringify(obj.metrics));
    }
  } catch (err) {
    log('POST failed:', err.message);
  } finally {
    clearTimeout(t);
  }
}

function handleLine(line) {
  const s = line.trim();
  if (!s) return;
  if (s[0] === '#') {
    log('gw:', s); // diagnostic line from the gateway
    return;
  }
  if (s[0] !== '{') {
    log('skip (not JSON):', s);
    return;
  }
  let obj;
  try {
    obj = JSON.parse(s);
  } catch (err) {
    log('bad JSON:', s);
    return;
  }
  if (!obj.device_id || typeof obj.metrics !== 'object' || obj.metrics === null) {
    log('missing device_id/metrics:', s);
    return;
  }
  postTelemetry(obj);
}

function connect() {
  log(`opening ${SERIAL_PORT} @ ${SERIAL_BAUD} -> ${SERVER_URL}`);
  const port = new SerialPort({ path: SERIAL_PORT, baudRate: SERIAL_BAUD }, (err) => {
    if (err) {
      log('open failed:', err.message, `- retrying in ${RECONNECT_MS}ms`);
      setTimeout(connect, RECONNECT_MS);
    }
  });

  const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));
  parser.on('data', handleLine);

  port.on('open', () => log('serial open'));
  port.on('error', (err) => log('serial error:', err.message));
  port.on('close', () => {
    log(`serial closed - reconnecting in ${RECONNECT_MS}ms`);
    setTimeout(connect, RECONNECT_MS);
  });
}

connect();
