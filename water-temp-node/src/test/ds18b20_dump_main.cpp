/*
 * ds18b20_dump_main.cpp — DS18B20 bring-up DIAGNOSTIC (STM32F103C8T6 Blue Pill).
 *
 * Same wiring/pins as ds18b20_test_main.cpp, but instead of a temperature it
 * prints the RAW 9-byte scratchpad plus the CRC verdict for each probe. Use it
 * when the normal test reports FAULT(crc) and you need to know why:
 *   FF FF FF ...  -> nothing drives the line (probe absent / DQ not connected)
 *   00 00 00 ...  -> line stuck low (short, or DQ shorted to GND)
 *   plausible + CRC bad -> slot-timing / pull-up / two devices on one wire
 * A healthy 12-bit idle probe looks like: <lo> <hi> 4B 46 7F FF 0C 10 <crc>.
 *
 * Built only by [env:bluepill_f103c8_dump]. Semihosting output over SWD.
 */
#include <Arduino.h>
#include <stdio.h>
#include "../ds18b20.h"

#define TEST_CONVERT_MS   750

static const ds_bus_t bus_hot  = { GPIOB, GPIO_PIN_6 };
static const ds_bus_t bus_cold = { GPIOB, GPIO_PIN_7 };

static inline int semihost_call(int op, void *arg)
{
    register int   r0 asm("r0") = op;
    register void *r1 asm("r1") = arg;
    asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory", "cc");
    return r0;
}
static void out(const char *s) { semihost_call(0x04 /*SYS_WRITE0*/, (void *)s); }

/* Same Maxim CRC-8 the driver uses, duplicated here so the dump can report the
 * expected value alongside the byte the probe sent. */
static uint8_t ds_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t in = data[i];
        for (int j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ in) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            in >>= 1;
        }
    }
    return crc;
}

static void dump_probe(const char *label, const ds_bus_t *bus)
{
    char line[128];
    uint8_t sp[9];

    int present = ds18b20_start_convert(bus);
    delay(TEST_CONVERT_MS);
    int answered = ds18b20_read_scratchpad(bus, sp);

    if (!present || !answered) {
        snprintf(line, sizeof(line), "%-4s presence=%d read=%d (no device answered)\r\n",
                 label, present, answered);
        out(line);
        return;
    }

    uint8_t want = ds_crc8(sp, 8);
    int16_t raw  = (int16_t)(((uint16_t)sp[1] << 8) | sp[0]);
    int32_t c100 = ((int32_t)raw * 100) / 16;
    snprintf(line, sizeof(line),
             "%-4s %02X %02X %02X %02X %02X %02X %02X %02X %02X | crc got=%02X want=%02X %s | %ld.%02ld C\r\n",
             label, sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7], sp[8],
             sp[8], want, (sp[8] == want) ? "OK " : "BAD",
             (long)(c100 / 100), (long)((c100 < 0 ? -c100 : c100) % 100));
    out(line);
}

/*
 * Electrical probe of one DQ pin, run once before any 1-Wire traffic. Reads the
 * idle line with the MCU's internal pull-up, then with its internal pull-down:
 *   up=1 down=1 -> a strong external pull-up (4.7k) is winning  -> wiring OK
 *   up=1 down=0 -> no external pull-up; only the internal ~40k holds it high
 *   up=0 down=0 -> the line is held LOW by something stronger than 40k
 *                  (DQ shorted to GND, or the pull-up resistor tied to GND)
 */
static void probe_pin(const char *label, const ds_bus_t *bus)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = bus->pin;
    g.Mode  = GPIO_MODE_INPUT;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(bus->port, &g);
    delay(2);
    int up = HAL_GPIO_ReadPin(bus->port, bus->pin) == GPIO_PIN_SET;

    g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(bus->port, &g);
    delay(2);
    int down = HAL_GPIO_ReadPin(bus->port, bus->pin) == GPIO_PIN_SET;

    const char *verdict = (up && down) ? "external pull-up present (OK)"
                        : (up)         ? "NO external pull-up (only internal ~40k)"
                                       : "LINE HELD LOW (short to GND?)";
    char line[96];
    snprintf(line, sizeof(line), "%-4s idle: pull-up=%d pull-down=%d -> %s\r\n",
             label, up, down, verdict);
    out(line);
}

void setup(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    ds18b20_init(&bus_hot);
    ds18b20_init(&bus_cold);

    out("\r\nDS18B20 RAW SCRATCHPAD DUMP | hot=PB6 cold=PB7\r\n");
#ifdef DS18B20_INTERNAL_PULLUP
    out("pull-up: internal ~40k (bench fallback)\r\n");
#else
    out("pull-up: external (GPIO_NOPULL)\r\n");
#endif
    out("healthy idle probe looks like: LO HI 4B 46 7F FF 0C 10 CRC\r\n");

    probe_pin("hot",  &bus_hot);
    probe_pin("cold", &bus_cold);
    ds18b20_init(&bus_hot);          /* probe_pin left the pins as inputs */
    ds18b20_init(&bus_cold);
}

void loop(void)
{
    dump_probe("hot",  &bus_hot);
    dump_probe("cold", &bus_cold);
    out("--\r\n");
    delay(500);
}
