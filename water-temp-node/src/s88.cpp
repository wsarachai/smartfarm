/*
 * s88.cpp — Senseair S88 LP CO2 over Modbus RTU / RS-485. See s88.h.
 *
 * Deliberately hand-rolled, like the other drivers here: one Modbus function
 * (0x03, read holding registers), no library, no dynamic allocation.
 */
#include "s88.h"
#include "node_config.h"

/* LPUART1 on PC1 (TX) / PC0 (RX). STM32duino's HardwareSerial takes (rx, tx)
 * and resolves the peripheral instance from the pin map. */
static HardwareSerial s88_serial(S88_UART_RX_PIN, S88_UART_TX_PIN);
static uint8_t        s88_started = 0;

/* Modbus RTU CRC16: poly 0xA001 (reflected 0x8005), init 0xFFFF.
 * NOT the frame's lora_crc8, and NOT the Sensirion CRC-8 in sensirion_i2c.cpp —
 * this file is the third distinct CRC in this codebase, so keep them straight. */
static uint16_t modbus_crc16(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) crc = (uint16_t)((crc >> 1) ^ 0xA001);
            else         crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

void s88_begin(void)
{
    if (s88_started) return;
    pinMode(S88_DE_PIN, OUTPUT);
    digitalWrite(S88_DE_PIN, LOW);      /* listen by default */
    s88_serial.begin(S88_UART_BAUD, SERIAL_8N1);
    s88_started = 1;
}

/* One request/response exchange. Returns 1 on a CRC-valid reply. */
static int s88_txn(uint16_t reg, uint16_t *out_val)
{
    uint8_t req[8];
    req[0] = S88_MODBUS_ADDR;
    req[1] = 0x03;                      /* read holding registers */
    req[2] = (uint8_t)(reg >> 8);
    req[3] = (uint8_t)(reg & 0xFF);
    req[4] = 0x00;
    req[5] = 0x01;                      /* one register */
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);     /* Modbus sends CRC low byte first */
    req[7] = (uint8_t)(crc >> 8);

    /* Drop anything stale before we talk. */
    while (s88_serial.available()) (void)s88_serial.read();

    digitalWrite(S88_DE_PIN, HIGH);     /* take the bus */
    s88_serial.write(req, sizeof(req));
    s88_serial.flush();                 /* BLOCKS until the last stop bit is out —
                                         * a delay() here instead would truncate
                                         * the frame. This is load-bearing. */
    digitalWrite(S88_DE_PIN, LOW);      /* release before the slave answers */

    /* With DE and !RE tied together the receiver is disabled while we transmit,
     * so there should be no echo — but a board wired with !RE grounded separately
     * will echo, and that is a common variation. Discard up to one frame's worth
     * if it is there. The slave must wait 3.5 character times (~3.6 ms at 9600)
     * before replying, so this cannot eat the response. */
    uint32_t t0 = millis();
    int      echoed = 0;
    while (echoed < (int)sizeof(req) && (millis() - t0) < 5) {
        if (s88_serial.available()) { (void)s88_serial.read(); echoed++; }
    }

    /* Expected reply: addr, 0x03, bytecount=2, hi, lo, crc_lo, crc_hi */
    uint8_t rsp[7];
    int     n = 0;
    t0 = millis();
    while (n < (int)sizeof(rsp)) {
        if (s88_serial.available()) {
            rsp[n++] = (uint8_t)s88_serial.read();
            t0 = millis();              /* inter-byte, not whole-frame, timeout */
        } else if ((millis() - t0) > S88_RESPONSE_TIMEOUT_MS) {
            return 0;
        }
    }

    if (rsp[0] != S88_MODBUS_ADDR) return 0;
    if (rsp[1] != 0x03)            return 0;   /* 0x83 would be an exception */
    if (rsp[2] != 0x02)            return 0;

    uint16_t got = (uint16_t)(rsp[5] | ((uint16_t)rsp[6] << 8));
    if (got != modbus_crc16(rsp, 5)) return 0;

    *out_val = (uint16_t)(((uint16_t)rsp[3] << 8) | rsp[4]);
    return 1;
}

int s88_read_co2(uint16_t *out_ppm)
{
    if (!out_ppm) return 0;
    s88_begin();

    for (int try_i = 0; try_i <= S88_RETRIES; try_i++) {
        uint16_t v;
        if (s88_txn(S88_CO2_REG, &v)) {
            /* The S88's specified range is 400-10000 ppm; it will report outside
             * that with degraded accuracy. Reject only the obviously broken
             * values, so a real high-CO2 greenhouse reading is not discarded. */
            if (v > 0 && v < 40000) { *out_ppm = v; return 1; }
        }
        delay(20);
    }
    return 0;
}
