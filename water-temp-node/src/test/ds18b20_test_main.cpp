/*
 * ds18b20_test_main.cpp — DS18B20 bring-up test on an STM32F103C8T6 (Blue Pill).
 *
 * Standalone sensor test used while the NUCLEO-WL55JC1 boards are on back-order:
 * it exercises the SAME ds18b20.cpp driver the real node ships, so a good read
 * here means the driver + wiring are sound before the WL55 hardware arrives.
 *
 * Built ONLY in the [env:bluepill_f103c8] env (see platformio.ini build_src_filter);
 * the WL55 env excludes src/test/. Reads two probes once per second and prints
 * over USB CDC (the Blue Pill's micro-USB). No LoRa, no sleep — just the sensor.
 *
 * Wiring (direct 3V3, no power-gate MOSFET):
 *   PB6 --[4.7k]--+-- DQ + VDD  (probe A = "hot")   ,  3V3
 *   PB7 --[4.7k]--+-- DQ + VDD  (probe B = "cold")  ,  3V3
 *   GND common. PC13 = onboard LED (active-low) heartbeat.
 *
 * Flash: pio run -e bluepill_f103c8 -t upload   (ST-Link on SWD)
 * View:  pio device monitor -e bluepill_f103c8  (USB CDC COM port)
 */
#include <Arduino.h>
#include "../ds18b20.h"

/* DS18B20 12-bit conversion time. Kept local so this test needs no WL headers. */
#define TEST_CONVERT_MS   750

static const ds_bus_t bus_hot  = { GPIOB, GPIO_PIN_6 };
static const ds_bus_t bus_cold = { GPIOB, GPIO_PIN_7 };

/* Print "<int>.<NN> C" for a centi value without relying on float printf. */
static void print_centi(int centi)
{
    int neg = centi < 0;
    int a = neg ? -centi : centi;
    if (neg) Serial.print('-');
    Serial.print(a / 100);
    Serial.print('.');
    if ((a % 100) < 10) Serial.print('0');
    Serial.print(a % 100);
    Serial.print(F(" C"));
}

/* Print one probe's result: a temperature or a specific FAULT reason. */
static void print_probe(const char *label, int presence, int read_ok, int16_t c100)
{
    Serial.print(label);
    Serial.print('=');
    if (!presence)      Serial.print(F("FAULT(no presence)"));
    else if (!read_ok)  Serial.print(F("FAULT(crc)"));
    else                print_centi(c100);
}

void setup(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();       /* probe data pins live on port B */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);    /* PC13 is active-low: HIGH = off */

    Serial.begin(115200);               /* USB CDC ignores the baud value */
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { /* wait up to 3s for the host */ }

    ds18b20_init(&bus_hot);
    ds18b20_init(&bus_cold);

    Serial.println();
    Serial.println(F("DS18B20 F103 test | hot=PB6 cold=PB7 | direct 3V3, 4.7k pull-ups"));
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

    print_probe("hot",  ph, okh, th);
    Serial.print(F("  "));
    print_probe("cold", pc, okc, tc);
    Serial.println();

    /* Heartbeat: brief LED flash when at least one probe read cleanly. */
    if (okh || okc) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(20);
        digitalWrite(LED_BUILTIN, HIGH);
    }

    delay(230);   /* ~1 Hz overall (750 + 20 + 230) */
}
