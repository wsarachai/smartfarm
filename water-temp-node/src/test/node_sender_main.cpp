/*
 * node_sender_main.cpp — F103C8T6 prototype NODE: DS18B20 + BME280 + SHT45 +
 * SCD41 -> 20-byte lora_packet v3 -> SX1278 LoRa TX. The counterpart to
 * lora-pi-receiver on the Raspberry Pi.
 *
 * Reuses the drivers already validated on this board: ds18b20.cpp (PB6/PB7),
 * sx1278.cpp (SPI1 + PA4/PB0/PB1), sht45.cpp + scd41.cpp (I2C2), and the shared
 * lora/lora_packet.h frame — the SAME frame the WL55 node sends, so the Pi
 * receiver decodes both identically. Prints status via semihosting over SWD.
 *
 * 433 MHz prototype (SX1278) — talks to the Pi's SX1278, NOT the 923 MHz WL55
 * gateway. PHY here MUST match lora-pi-receiver/.env.
 *
 * Sensor split (this board carries both a BME280 and an SHT45):
 *   air_temp + humidity  <- SHT45   (+-0.1 degC / +-1 %RH, beats the BME280)
 *   pressure             <- BME280  (the SHT45 has no barometer)
 *   co2                  <- SCD41
 * LORA_FLAG_SHT is set when the SHT45 supplied T/RH; if it fails to answer we
 * fall back to the BME280 for those two and clear the flag, so a dead SHT45
 * costs accuracy but not telemetry.
 *
 * Wiring: DS18B20 hot=PB6 cold=PB7 (direct 3V3, external 4.7k pull-up DQ->3V3;
 *         selected by `ds18b20_pullup` in platformio.ini);
 *         I2C2 PB10=SCL PB11=SDA, shared by BME280 (0x76), SHT45 (0x44) and
 *         SCD41 (0x62) — one pair of 4.7k pull-ups for the whole bus;
 *         SX1278 NSS=PA4 SCK=PA5 MISO=PA6 MOSI=PA7 RESET=PB0 DIO0=PB1, 3V3,
 *         antenna.
 *
 * Power note: the SCD41 pulls ~205 mA in peaks. Run this board from a supply
 * with real headroom — a marginal 3V3 LDO browns out mid-measurement and the
 * symptom looks like a flaky I2C bus.
 */
#include <Arduino.h>
#include <stdio.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

#include "../ds18b20.h"
#include "../sht45.h"
#include "../scd41.h"
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

/* Shared I2C2 (PB10=SCL, PB11=SDA) — I2C1/PB6-7 is taken by the DS18B20 probes. */
#define BME_ADDR   0x76
static TwoWire        Wire2(PB11, PB10);   /* (SDA, SCL) */
static Adafruit_BME280 bme;
static bool           bme_ok = false;
static bool           sht_ok = false;
static bool           scd_ok = false;

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

/* Last good CO2 reading. The SCD41 produces one every 5 s in periodic mode but
 * we transmit every 10 s, so there is normally a fresh one — this only covers
 * the first loop and the occasional miss. */
static uint16_t co2_last  = 0;
static bool     co2_valid = false;

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

    out("\r\nF103 NODE sender | DS18B20 PB6/PB7 + BME280/SHT45/SCD41 on I2C2"
        " -> 20B frame v3 -> SX1278 433MHz\r\n");
#ifdef DS18B20_INTERNAL_PULLUP
    out("DS18B20 bus: internal ~40k pull-up (bench fallback)\r\n");
#else
    out("DS18B20 bus: external 4.7k pull-up\r\n");
#endif

    ds18b20_init(&bus_hot);
    ds18b20_init(&bus_cold);

    Wire2.begin();
    Wire2.setClock(100000);

    bme_ok = bme.begin(BME_ADDR, &Wire2);
    out(bme_ok ? "BME280 (0x76) -> OK (pressure)\r\n"
               : "BME280 -> not found (check I2C2 PB10/PB11 + addr 0x76/0x77)\r\n");

    sht_ok = sht45_init(&Wire2, SHT45_I2C_ADDR) != 0;
    out(sht_ok ? "SHT45  (0x44) -> OK (air temp + humidity)\r\n"
               : "SHT45  -> not found (falling back to BME280 for temp/RH)\r\n");

    /* The SCD41 needs ~1 s after power-up before it accepts commands. Everything
     * above has burned some of that already; top it up so a cold boot is safe. */
    delay(SCD41_POWER_UP_MS);

    scd_ok = scd41_init(&Wire2, SCD41_I2C_ADDR) != 0;
    if (scd_ok) {
        /* Free-running 5 s measurements: this board is mains/USB powered and
         * awake continuously, so periodic mode is both simpler and more accurate
         * than the single-shot path the battery-powered WL55 node uses. */
        scd_ok = scd41_start_periodic() != 0;
    }
    out(scd_ok ? "SCD41  (0x62) -> OK (CO2, periodic 5s)\r\n"
               : "SCD41  -> not found / would not start (check 0x62 + supply)\r\n");

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

    /* --- BME280: pressure only (the SHT45 owns temp/RH when it is present) --- */
    uint16_t prs = 0;
    int16_t  bme_air = 0;
    uint16_t bme_hum = 0;
    if (bme_ok) {
        prs     = (uint16_t)lroundf(bme.readPressure() / 10.0f);   /* Pa/10 = deci-hPa */
        bme_air = (int16_t)lroundf(bme.readTemperature() * 100.0f);
        bme_hum = (uint16_t)lroundf(bme.readHumidity() * 100.0f);
    }

    /* --- SHT45: air temp + humidity, falling back to the BME280 --- */
    int16_t  air = 0;
    uint16_t hum = 0;
    bool     from_sht = false;
    if (sht_ok && sht45_read(&air, &hum)) {
        from_sht = true;
    } else if (bme_ok) {
        air = bme_air;
        hum = bme_hum;
    }
    bool air_ok = from_sht || bme_ok;

    /* --- SCD41: CO2 --- */
    if (scd_ok) {
        int ready = 0;
        if (scd41_data_ready(&ready) && ready) {
            uint16_t c = 0;
            if (scd41_read(&c, 0, 0) && c > 0) {
                co2_last  = c;
                co2_valid = true;
            }
        }
        /* Feed the measured pressure back for CO2 density compensation — worth
         * roughly 1 % of reading per 10 hPa, so it is a real correction, not a
         * flourish. Cheap enough to redo every cycle. */
        if (bme_ok && prs > 0) scd41_set_ambient_pressure((uint16_t)(prs / 10));
    }

    /* --- pack the 20-byte v3 frame --- */
    lora_payload_t p;
    p.node_id        = NODE_ID;
    p.seq            = seq++;
    p.flags          = 0;
    p.temp_hot_c100  = okh ? th : LORA_TEMP_INVALID;
    p.temp_cold_c100 = okc ? tc : LORA_TEMP_INVALID;
    p.battery_mv     = vmv;
    p.air_temp_c100  = air;
    p.humidity_x100  = hum;
    p.pressure_dhpa  = prs;
    p.co2_ppm        = co2_valid ? co2_last : 0;
    if (okh)       p.flags |= LORA_FLAG_HOT;
    if (okc)       p.flags |= LORA_FLAG_COLD;
    if (vmv)       p.flags |= LORA_FLAG_BATT;
    if (air_ok)    p.flags |= (LORA_FLAG_AIR | LORA_FLAG_HUM);
    if (from_sht)  p.flags |= LORA_FLAG_SHT;
    if (bme_ok)    p.flags |= LORA_FLAG_PRESS;
    if (co2_valid) p.flags |= LORA_FLAG_CO2;

    uint8_t buf[LORA_PKT_LEN_V3];
    lora_packet_pack_v3(&p, buf);

    /* --- transmit --- */
    char line[160];
    if (radio_ok && sx127x_tx(buf, LORA_PKT_LEN_V3, 2000)) {
        snprintf(line, sizeof(line),
                 "TX seq=%u hot=%d/%d cold=%d/%d air=%d(%s) hum=%u prs=%u co2=%u -> TxDone\r\n",
                 (unsigned)p.seq, (int)th, okh, (int)tc, okc,
                 (int)air, from_sht ? "sht" : "bme",
                 (unsigned)hum, (unsigned)prs, (unsigned)p.co2_ppm);
        out(line);
        digitalWrite(LED_BUILTIN, LOW); delay(20); digitalWrite(LED_BUILTIN, HIGH);
    } else {
        out(radio_ok ? "TX TIMEOUT\r\n" : "radio not ready\r\n");
    }

    delay(TX_INTERVAL_MS);
}
