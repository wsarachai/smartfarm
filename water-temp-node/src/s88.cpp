/*
 * s88.cpp — Senseair S88 CO2 over Modbus RTU / RS-485. See s88.h.
 *
 * Deliberately hand-rolled, like the other drivers here: three Modbus functions
 * (0x04 read input registers, 0x03 read holding registers, 0x06 write single
 * register), no library, no dynamic allocation.
 *
 * Frames are exactly the ones in TDE14367 §8 "Application examples", e.g. the
 * status+CO2 read is <FE><04><00><00><00><04><E5><C6> and the sensor answers
 * <FE><04><08><status hi><lo><alarm hi><lo><output hi><lo><co2 hi><lo><crc>.
 */
#include <string.h>
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

/* Send one request and collect a reply of exactly rsp_len bytes.
 * Returns 1 on a CRC-valid, address-matching reply with the expected function
 * code; 0 otherwise (timeout, CRC, exception 0x8x, wrong shape). */
static int s88_exchange(const uint8_t *req, int req_len,
                        uint8_t *rsp, int rsp_len)
{
    /* Drop anything stale before we talk. */
    while (s88_serial.available()) (void)s88_serial.read();

    digitalWrite(S88_DE_PIN, HIGH);     /* take the bus */
    s88_serial.write(req, req_len);
    s88_serial.flush();                 /* BLOCKS until the last stop bit is out —
                                         * a delay() here instead would truncate
                                         * the frame. This is load-bearing. */
    digitalWrite(S88_DE_PIN, LOW);      /* release before the slave answers */

    /* With DE and !RE tied together the receiver is disabled while we transmit,
     * so there should be no echo — but a board wired with !RE grounded separately
     * will echo, and that is a common variation. Discard up to one frame's worth
     * if it is there. The slave must wait 3.5 character times (~4 ms at 9600,
     * 11-bit characters) before replying, so this cannot eat the response. */
    uint32_t t0 = millis();
    int      echoed = 0;
    while (echoed < req_len && (millis() - t0) < 3) {
        if (s88_serial.available()) { (void)s88_serial.read(); echoed++; }
    }

    /* TDE14367 §5: the sensor's response time-out is 180 ms max, so the wait
     * for the FIRST byte must be at least that. Between bytes we allow the same
     * budget — generous, but a 5 m cable in a greenhouse earns generosity. */
    int n = 0;
    t0 = millis();
    while (n < rsp_len) {
        if (s88_serial.available()) {
            rsp[n++] = (uint8_t)s88_serial.read();
            t0 = millis();
        } else if ((millis() - t0) > S88_RESPONSE_TIMEOUT_MS) {
            return 0;
        }
    }

    if (rsp[0] != req[0]) return 0;         /* address (0xFE echoes as 0xFE) */
    if (rsp[1] != req[1]) return 0;         /* 0x8x here would be an exception */
    uint16_t got = (uint16_t)(rsp[rsp_len - 2] | ((uint16_t)rsp[rsp_len - 1] << 8));
    return got == modbus_crc16(rsp, rsp_len - 2);
}

static void put_crc(uint8_t *frame, int len_without_crc)
{
    uint16_t crc = modbus_crc16(frame, len_without_crc);
    frame[len_without_crc]     = (uint8_t)(crc & 0xFF);   /* low byte first */
    frame[len_without_crc + 1] = (uint8_t)(crc >> 8);
}

/* Read `count` consecutive 16-bit registers with function `fc` (0x03 or 0x04). */
static int s88_read_regs(uint8_t fc, uint16_t addr, uint8_t count, uint16_t *out)
{
    if (count == 0 || count > 8) return 0;
    uint8_t req[8] = { S88_MODBUS_ADDR, fc,
                       (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
                       0x00, count, 0, 0 };
    put_crc(req, 6);

    uint8_t rsp[3 + 2 * 8 + 2];
    int     rsp_len = 3 + 2 * count + 2;
    if (!s88_exchange(req, sizeof(req), rsp, rsp_len)) return 0;
    if (rsp[2] != (uint8_t)(2 * count)) return 0;

    for (int i = 0; i < count; i++)
        out[i] = (uint16_t)(((uint16_t)rsp[3 + 2 * i] << 8) | rsp[4 + 2 * i]);
    return 1;
}

/* Write one holding register (function 0x06). The reply is an echo. */
static int s88_write_reg(uint16_t addr, uint16_t val)
{
    uint8_t req[8] = { S88_MODBUS_ADDR, 0x06,
                       (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
                       (uint8_t)(val >> 8),  (uint8_t)(val & 0xFF), 0, 0 };
    put_crc(req, 6);
    uint8_t rsp[8];
    if (!s88_exchange(req, sizeof(req), rsp, sizeof(rsp))) return 0;
    return memcmp(rsp, req, 6) == 0;
}

int s88_read_co2(uint16_t *out_ppm, uint16_t *out_status)
{
    if (!out_ppm) return 0;
    s88_begin();

    for (int try_i = 0; try_i <= S88_RETRIES; try_i++) {
        /* IR1..IR4 in one frame: MeterStatus, AlarmStatus, OutputStatus, CO2.
         * One exchange instead of two, and status and value are guaranteed to
         * come from the same measurement period. TDE14367 §8.3. */
        uint16_t ir[4];
        if (s88_read_regs(0x04, S88_IR_STATUS, 4, ir)) {
            uint16_t status = ir[0];
            uint16_t ppm    = ir[3];
            if (out_status) *out_status = status;

            /* Warm-up, self-diagnostic, out-of-range etc. all mean "this number
             * is not a measurement". Do not retry — the sensor answered, it just
             * has nothing valid yet. The caller keeps its cache. */
            if (status & S88_ST_BAD) return 0;

            /* Specified range is 400-10000 ppm (LP) / 0-20000 (GH); anything the
             * status word did not already reject is a real reading. Guard only
             * against the obviously broken. */
            if (ppm < 40000) { *out_ppm = ppm; return 1; }
            return 0;
        }
        delay(20);
    }
    return 0;
}

int s88_apply_site_pressure(int16_t dhpa)
{
    s88_begin();
    uint16_t cur;
    if (!s88_read_regs(0x03, S88_HR_DEFAULT_PRESSURE, 1, &cur)) return 0;
    if ((int16_t)cur == dhpa) return 1;          /* already right: no EEPROM write */

    if (!s88_write_reg(S88_HR_DEFAULT_PRESSURE, (uint16_t)dhpa)) return 0;
    delay(200);   /* TDE14367 §3 note: >=180 ms before anything else after an
                   * EEPROM-mapped write, or the write can corrupt. */
    /* HR27 only takes effect at the next power-up; HR4 is the live value. Write
     * both so the very next reading is already compensated. HR4 is RAM. */
    (void)s88_write_reg(S88_HR_PRESSURE, (uint16_t)dhpa);
    return 1;
}
