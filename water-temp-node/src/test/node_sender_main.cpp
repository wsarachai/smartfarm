/*
 * node_sender_main.cpp — F103C8T6 prototype NODE: DS18B20 -> 12-byte frame ->
 * SX1278 LoRa TX. The counterpart to lora-pi-receiver on the Raspberry Pi.
 *
 * Reuses the drivers already validated on this board: ds18b20.cpp (PB6/PB7),
 * sx1278.cpp (SPI1 + PA4/PB0/PB1), battery.cpp (VREFINT), and the shared
 * lora/lora_packet.h frame — the SAME frame the WL55 node would send, so the Pi
 * receiver decodes it identically. Prints status via semihosting over SWD.
 *
 * 433 MHz prototype (SX1278) — talks to the Pi's SX1278, NOT the 923 MHz WL55
 * gateway. PHY here MUST match lora-pi-receiver/.env.
 *
 * Wiring: DS18B20 hot=PB6 cold=PB7 (direct 3V3, internal pull-up); SX1278
 * NSS=PA4 SCK=PA5 MISO=PA6 MOSI=PA7 RESET=PB0 DIO0=PB1, 3V3, antenna.
 */
#include <Arduino.h>
#include <stdio.h>

#include "../ds18b20.h"
#include "../sx1278.h"
#include "../lora/lora_packet.h"
/* NOTE: no battery here — STM32F1 has no factory VREFINT calibration, so
 * battery.cpp (VREFINT, used by the WL55 node) doesn't build on F103. This
 * prototype sends battery_mv=0 (flag off); the Pi receiver handles that. */

#define NODE_ID        1
#define TX_INTERVAL_MS 10000
#define DS_CONVERT_MS  750

/* DS18B20 buses */
static const ds_bus_t bus_hot  = { GPIOB, GPIO_PIN_6 };
static const ds_bus_t bus_cold = { GPIOB, GPIO_PIN_7 };

/* SX1278 pins + PHY (must match lora-pi-receiver) */
#define SX_NSS    PA4
#define SX_RESET  PB0
#define SX_DIO0   PB1
#define SX_FREQ_HZ   433000000UL
#define SX_SF        9
#define SX_BW_CODE   7      /* 125 kHz */
#define SX_CR_CODE   1      /* 4/5     */
#define SX_PREAMBLE  8
#define SX_SYNCWORD  0x12
#define SX_POWER_DBM 17

/* ---- semihosting / Serial output (as the other F103 tests) ------------------ */
#ifdef USE_SEMIHOSTING
static inline int semihost_call(int op, void *arg)
{
    register int   r0 asm("r0") = op;
    register void *r1 asm("r1") = arg;
    asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory", "cc");
    return r0;
}
static void out(const char *s) { semihost_call(0x04, (void *)s); }
#else
static void out(const char *s) { Serial.print(s); }
#endif

static bool     radio_ok = false;
static uint8_t  seq = 0;

void setup(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

#ifndef USE_SEMIHOSTING
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { }
#endif

    out("\r\nF103 NODE sender | DS18B20 PB6/PB7 -> 12B frame -> SX1278 433MHz SF9\r\n");

    ds18b20_init(&bus_hot);
    ds18b20_init(&bus_cold);

    sx127x_pins_t pins = { SX_NSS, SX_RESET, SX_DIO0 };
    radio_ok = sx127x_begin(&pins, SX_FREQ_HZ);

    char line[80];
    snprintf(line, sizeof(line), "SX1278 RegVersion=0x%02X -> %s\r\n",
             sx127x_read_version(), radio_ok ? "OK" : "FAULT");
    out(line);
    if (radio_ok) {
        sx127x_config_lora(SX_SF, SX_BW_CODE, SX_CR_CODE, SX_PREAMBLE,
                           SX_SYNCWORD, true, SX_POWER_DBM);
    } else {
        out("radio FAULT: check SX1278 SPI wiring\r\n");
    }
}

void loop(void)
{
    /* --- read both probes --- */
    int ph = ds18b20_start_convert(&bus_hot);
    int pc = ds18b20_start_convert(&bus_cold);
    delay(DS_CONVERT_MS);
    int16_t th = 0, tc = 0;
    int okh = ph && ds18b20_read(&bus_hot,  &th);
    int okc = pc && ds18b20_read(&bus_cold, &tc);

    uint16_t vmv = 0;   /* battery not measured on F103 (no VREFINT cal) */

    /* --- pack the shared 12-byte frame --- */
    lora_payload_t p;
    p.node_id        = NODE_ID;
    p.seq            = seq++;
    p.flags          = 0;
    p.temp_hot_c100  = okh ? th : LORA_TEMP_INVALID;
    p.temp_cold_c100 = okc ? tc : LORA_TEMP_INVALID;
    p.battery_mv     = vmv;
    if (okh) p.flags |= LORA_FLAG_HOT;
    if (okc) p.flags |= LORA_FLAG_COLD;
    if (vmv) p.flags |= LORA_FLAG_BATT;

    uint8_t buf[LORA_PKT_LEN];
    lora_packet_pack(&p, buf);

    /* --- transmit --- */
    char line[96];
    if (radio_ok && sx127x_tx(buf, LORA_PKT_LEN, 2000)) {
        snprintf(line, sizeof(line),
                 "TX seq=%u hot=%d/%d cold=%d/%d batt=%umV -> TxDone\r\n",
                 (unsigned)p.seq, (int)th, okh, (int)tc, okc, (unsigned)vmv);
        out(line);
        digitalWrite(LED_BUILTIN, LOW); delay(20); digitalWrite(LED_BUILTIN, HIGH);
    } else {
        out(radio_ok ? "TX TIMEOUT\r\n" : "radio not ready\r\n");
    }

    delay(TX_INTERVAL_MS);
}
