/*
 * ds18b20_test_main.cpp — DS18B20 bring-up test on an STM32F103C8T6 (Blue Pill).
 *
 * Standalone sensor test used while the NUCLEO-WL55JC1 boards are on back-order:
 * it exercises the SAME ds18b20.cpp driver the real node ships, so a good read
 * here means the driver + wiring are sound before the WL55 hardware arrives.
 *
 * Built ONLY in the F103 envs (see platformio.ini build_src_filter); the WL55 env
 * excludes src/test/. Reads two probes once per second. Output goes to whichever
 * path the env selects:
 *   [env:bluepill_f103c8]              -> USB CDC 'Serial' (the board's micro-USB)
 *   [env:bluepill_f103c8_semihosting]  -> semihosting over SWD (-DUSE_SEMIHOSTING),
 *                                         viewed in the OpenOCD console via the
 *                                         ST-Link. No USB / UART / adapter needed.
 *
 * Wiring (direct 3V3, no power-gate MOSFET):
 *   PB6 --[4.7k]--+-- DQ + VDD  (probe A = "hot")   ,  3V3
 *   PB7 --[4.7k]--+-- DQ + VDD  (probe B = "cold")  ,  3V3
 *   GND common. PC13 = onboard LED (active-low) heartbeat.
 */
#include <Arduino.h>
#include <stdio.h>
#include "../ds18b20.h"

#define TEST_CONVERT_MS   750

static const ds_bus_t bus_hot  = { GPIOB, GPIO_PIN_6 };
static const ds_bus_t bus_cold = { GPIOB, GPIO_PIN_7 };

/* ---- output abstraction: semihosting (SWD) or USB CDC 'Serial' -------------- */
#ifdef USE_SEMIHOSTING
/* ARM semihosting call: BKPT 0xAB with r0=op, r1=arg. Requires a debugger
 * (OpenOCD with `arm semihosting enable`) to be attached and running the target;
 * without one the BKPT faults, so this env is for on-the-bench viewing via SWD. */
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

/* Format one probe's result into dst: a temperature, or a specific FAULT reason. */
static void format_probe(char *dst, int cap, const char *label,
                         int presence, int read_ok, int16_t c100)
{
    if (!presence) {
        snprintf(dst, cap, "%s=FAULT(no presence)", label);
    } else if (!read_ok) {
        snprintf(dst, cap, "%s=FAULT(crc)", label);
    } else {
        int neg = c100 < 0;
        int a = neg ? -c100 : c100;
        snprintf(dst, cap, "%s=%s%d.%02d C", label, neg ? "-" : "", a / 100, a % 100);
    }
}

void setup(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();       /* probe data pins live on port B */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);    /* PC13 is active-low: HIGH = off */

#ifndef USE_SEMIHOSTING
    Serial.begin(115200);               /* USB CDC ignores the baud value */
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { /* wait up to 3s for the host */ }
#endif

    ds18b20_init(&bus_hot);
    ds18b20_init(&bus_cold);

    out("\r\nDS18B20 F103 test | hot=PB6 cold=PB7 | direct 3V3, 4.7k pull-ups\r\n");
}

void loop(void)
{
    /* Kick off both conversions, then wait once for the longer 12-bit time. */
    int ph = ds18b20_start_convert(&bus_hot);
    int pc = ds18b20_start_convert(&bus_cold);
    delay(TEST_CONVERT_MS);

    int16_t th = 0, tc = 0;
    int okh = ph && ds18b20_read(&bus_hot,  &th);
    int okc = pc && ds18b20_read(&bus_cold, &tc);

    char lh[32], lc[32], line[80];
    format_probe(lh, sizeof(lh), "hot",  ph, okh, th);
    format_probe(lc, sizeof(lc), "cold", pc, okc, tc);
    snprintf(line, sizeof(line), "%s  %s\r\n", lh, lc);
    out(line);

    /* Heartbeat: brief LED flash when at least one probe read cleanly. */
    if (okh || okc) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(20);
        digitalWrite(LED_BUILTIN, HIGH);
    }

    delay(230);   /* ~1 Hz overall (750 + 20 + 230) */
}
