/*
 * sx1278_test_main.cpp — SX1278 (SX127x) LoRa bench self-test on an F103C8T6.
 *
 * Used while the NUCLEO-WL55JC1 boards are on back-order, to validate an external
 * SX1278 module + its SPI wiring. It: resets the module, verifies SPI by reading
 * RegVersion (expect 0x12), configures the LoRa modem at 433 MHz, and transmits a
 * beacon every ~3 s (define SX1278_RX_MODE to listen instead, for a 2nd module).
 *
 * SX1278 is a 433 MHz (low-band) part -> this does NOT reach the 923 MHz WL55
 * gateway; it's a standalone module/wiring check. See src/sx1278.{h,cpp}.
 *
 * Built only in [env:bluepill_f103c8_sx1278]. Output via semihosting over SWD
 * (same as the DS18B20 semihosting env) — no USB/UART needed.
 *
 * Wiring (SX1278 module -> Blue Pill), 3V3 ONLY, antenna required before TX:
 *   VCC->3V3  GND->GND  NSS->PA4  SCK->PA5  MISO->PA6  MOSI->PA7
 *   RESET->PB0  DIO0->PB1  ANT->antenna
 */
#include <Arduino.h>
#include <stdio.h>
#include "../sx1278.h"

/* Pin map — see the wiring comment above. */
#define SX_NSS    PA4
#define SX_RESET  PB0
#define SX_DIO0   PB1

#define SX_FREQ_HZ   433000000UL   /* SX1278 low band */
#define SX_SF        9
#define SX_BW_CODE   7             /* 125 kHz */
#define SX_CR_CODE   1             /* 4/5     */
#define SX_PREAMBLE  8
#define SX_SYNCWORD  0x12          /* private */
#define SX_POWER_DBM 17

/* ---- output: semihosting (SWD) or USB CDC 'Serial', matching the other test -- */
#ifdef USE_SEMIHOSTING
static inline int semihost_call(int op, void *arg)
{
    register int   r0 asm("r0") = op;
    register void *r1 asm("r1") = arg;
    asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory", "cc");
    return r0;
}
static void out(const char *s) { semihost_call(0x04 /*SYS_WRITE0*/, (void *)s); }
#else
static void out(const char *s) { Serial.print(s); }
#endif

static bool     radio_ok = false;
static uint32_t seq = 0;

void setup(void)
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);    /* PC13 active-low: off */

#ifndef USE_SEMIHOSTING
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { }
#endif

    out("\r\nSX1278 LoRa self-test | 433 MHz SF9 BW125 CR4/5 | NSS=PA4 RST=PB0 DIO0=PB1\r\n");

    sx127x_pins_t pins = { SX_NSS, SX_RESET, SX_DIO0 };
    radio_ok = sx127x_begin(&pins, SX_FREQ_HZ);

    char line[80];
    uint8_t ver = sx127x_read_version();
    snprintf(line, sizeof(line),
             "RegVersion=0x%02X (expect 0x12) -> %s\r\n",
             ver, radio_ok ? "OK" : "FAULT");
    out(line);

    if (!radio_ok) {
        out("SPI/wiring FAULT: check NSS/SCK/MISO/MOSI/RESET + 3V3 (not 5V)\r\n");
        return;
    }

    sx127x_config_lora(SX_SF, SX_BW_CODE, SX_CR_CODE, SX_PREAMBLE,
                       SX_SYNCWORD, true, SX_POWER_DBM);
#ifdef SX1278_RX_MODE
    sx127x_rx_start();
    out("mode: RX (listening)\r\n");
#else
    out("mode: TX beacon every ~3s\r\n");
#endif
}

void loop(void)
{
    if (!radio_ok) { delay(1000); return; }

    char line[96];

#ifdef SX1278_RX_MODE
    uint8_t buf[64], len = 0;
    int rssi = 0; float snr = 0.0f;
    int r = sx127x_rx_poll(buf, sizeof(buf) - 1, &len, &rssi, &snr);
    if (r == 1) {
        buf[len] = 0;
        int st = (int)(snr * 10.0f + (snr >= 0 ? 0.5f : -0.5f));
        snprintf(line, sizeof(line), "RX %uB rssi=%d snr=%s%d.%d : %s\r\n",
                 (unsigned)len, rssi, st < 0 ? "-" : "",
                 (st < 0 ? -st : st) / 10, (st < 0 ? -st : st) % 10, (char *)buf);
        out(line);
        digitalWrite(LED_BUILTIN, LOW); delay(20); digitalWrite(LED_BUILTIN, HIGH);
    } else if (r == -1) {
        out("RX crc error\r\n");
    }
    delay(10);
#else
    char pkt[32];
    int n = snprintf(pkt, sizeof(pkt), "F103-SX1278 seq=%lu", (unsigned long)seq);
    bool ok = sx127x_tx((const uint8_t *)pkt, (uint8_t)n, 2000);
    snprintf(line, sizeof(line), "TX seq=%lu (%dB) -> %s\r\n",
             (unsigned long)seq, n, ok ? "TxDone" : "TIMEOUT");
    out(line);
    if (ok) { digitalWrite(LED_BUILTIN, LOW); delay(20); digitalWrite(LED_BUILTIN, HIGH); }
    seq++;
    delay(3000);
#endif
}
