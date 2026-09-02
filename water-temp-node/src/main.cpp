/*
 * water-temp-node — main.cpp  (STM32duino / Arduino framework)
 *
 * NUCLEO-WL55JC1 battery sensor node. Every WAKE_INTERVAL_S it wakes from Stop2,
 * powers the sensor rail through the AO3401A P-MOSFET, reads water temperature
 * (DS_PROBE_COUNT x DS18B20, one per pin), air temperature + humidity
 * (SHT45_COUNT x SHT45, behind a TCA9548A bus switch because they share one
 * factory-fixed I2C address)
 * and CO2 (Senseair S88 LP over Modbus/RS-485), measures the 24 V supply through
 * a divider, transmits ONE compact LoRa uplink (AS923), then sleeps. Uplink-only.
 *
 * The DS18B20s and SHT45s sit on the ONE gated rail, so that part of the
 * front-end is off between wakes. The S88 does NOT — it runs continuously from
 * its own 5.1 V rail, because its ABC needs continuous operation and 24 V solar
 * can afford it. The probes are nearly
 * free by comparison: each owns its own pin, so all six convert in PARALLEL
 * and the rail-on time is one 750 ms conversion regardless of how many are
 * fitted.
 *
 * Deep sleep (Stop2) + the RTC wake timer are done with RAW HAL here (not the
 * STM32duino Low Power/RTC libraries — those 2.0.0 releases #error against the
 * core PlatformIO bundles). STM32duino IS the Cube HAL, so HAL_PWREx/HAL_RTC are
 * available. The LoRa driver (src/lora/subghz_lora.c) is our HAL_SUBGHZ driver.
 */
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" {
#include "lora/subghz_lora.h"
}
#include "lora/lora_packet.h"
#include "ds18b20.h"
#include "sht45.h"
#include "tca9548a.h"
#include "s88.h"
#include "battery.h"
#include "node_config.h"

/* Provided (weak) by the STM32duino variant; we call it to restore the clock
 * tree after Stop2, which drops the core back to MSI. */
extern "C" void SystemClock_Config(void);

static RTC_HandleTypeDef hrtc;

/* One 1-Wire bus per probe; index == probe index on the wire and in the docs. */
static const ds_bus_t probe_bus[DS_PROBE_COUNT] = DS_PROBE_BUSES;
static_assert(DS_PROBE_COUNT <= LORA_PROBE_MAX,
              "DS_PROBE_COUNT exceeds the probe slots the LoRa frame carries");
static_assert(DS_PROBE_COUNT >= 2,
              "probes 0 and 1 fill the frame's original two slots; keep both");

static uint8_t seq = 0;

/* The three SHT45s are the only things on this bus, each behind its own
 * TCA9548A channel and its own <=5 m cable. Shares the probes' gated rail. */
static TwoWire i2c_sens(I2C_SDA_PIN, I2C_SCL_PIN);

static const uint8_t sht_mux_ch[SHT45_COUNT] = I2C_MUX_CHANNELS;
static_assert(SHT45_COUNT <= LORA_AIR_MAX,
              "SHT45_COUNT exceeds the air-sensor slots the LoRa frame carries");

/*
 * Connect air sensor i to the bus, and nothing else.
 *
 * This is the ONE place that knows how the three same-address SHT45s are kept
 * apart. Swap the body if the hardware changes (discrete bus switches, three
 * separate buses); nothing else in this file cares.
 */
static int sht45_select(int i)
{
    if (i < 0) return tca9548a_none(&i2c_sens, I2C_MUX_ADDR);
    if (i >= SHT45_COUNT) return 0;
    return tca9548a_select(&i2c_sens, I2C_MUX_ADDR, sht_mux_ch[i]);
}

/*
 * CO2 is now read EVERY wake — the S88 is always powered, so there is nothing to
 * pace. The cache remains for a different reason: to ride out a failed Modbus
 * exchange (a snagged cable, a wet connector) by re-sending the last good value
 * rather than dropping the metric. RAM survives Stop2.
 */
static uint16_t co2_last   = 0;
static uint8_t  co2_valid  = 0;
/* Site pressure (S88_SITE_PRESSURE_DHPA) is pushed into the sensor's EEPROM
 * once; this remembers that it succeeded so we do not re-read HR27 every wake.
 * Retried on each wake until the sensor has answered once. */
static uint8_t  co2_site_ok = (S88_SITE_PRESSURE_DHPA == 0);

/* -------------------------------------------------------------------------- */
static void dbgf(const char *fmt, ...)
{
#if defined(DEBUG_UART_ENABLED)
    char b[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    Serial.print(b);
#else
    (void)fmt;
#endif
}

/* -------------------------------------------------------------------------- */
/* Sensor-rail power gate (AO3401A P-MOSFET, active-low). */
static void gate_pin_init(void)
{
    DS_PWR_GPIO_CLK();
    GPIO_InitTypeDef g = {0};
    g.Pin   = DS_PWR_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DS_PWR_PORT, &g);
    HAL_GPIO_WritePin(DS_PWR_PORT, DS_PWR_PIN, DS_PWR_OFF_LEVEL);
}
static void gate_on(void)  { HAL_GPIO_WritePin(DS_PWR_PORT, DS_PWR_PIN, DS_PWR_ON_LEVEL);  }
static void gate_off(void) { HAL_GPIO_WritePin(DS_PWR_PORT, DS_PWR_PIN, DS_PWR_OFF_LEVEL); }

/*
 * Park every sensor pin as analog while the rail is unpowered.
 *
 * This is not just tidiness: with the rail off, a pin left driving or holding a
 * pull-up back-feeds the sensors through their ESD clamp diodes, which both
 * wastes the battery and half-powers the parts. The I2C lines matter as much as
 * the 1-Wire ones here — their 4.7k pull-ups live on the switched VSENS rail
 * (see docs/hardware-interface.md), so the MCU must not substitute its own.
 */
static void sensor_pins_park(void)
{
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    for (int i = 0; i < DS_PROBE_COUNT; i++) {
        g.Pin = probe_bus[i].pin;
        HAL_GPIO_Init(probe_bus[i].port, &g);
    }

    g.Pin = STM_LL_GPIO_PIN(digitalPinToPinName(I2C_SDA_PIN));
    HAL_GPIO_Init(get_GPIO_Port(STM_PORT(digitalPinToPinName(I2C_SDA_PIN))), &g);
    g.Pin = STM_LL_GPIO_PIN(digitalPinToPinName(I2C_SCL_PIN));
    HAL_GPIO_Init(get_GPIO_Port(STM_PORT(digitalPinToPinName(I2C_SCL_PIN))), &g);
}

/* -------------------------------------------------------------------------- */
/* Enable LSE + point the RTC at it, WITHOUT touching the core's sysclk/PLL. */
static void rtc_clock_init(void)
{
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON;
    osc.PLL.PLLState   = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    RCC_PeriphCLKInitTypeDef pclk = {0};
    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pclk.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&pclk);

    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();
}

static void rtc_init(void)
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;      /* LSE 32768 / 128 / 256 = 1 Hz */
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    hrtc.Init.OutPutRemap    = RTC_OUTPUT_REMAP_NONE;
    HAL_RTC_Init(&hrtc);

    /* CM4 core has a dedicated RTC wakeup line (RTC_WKUP_IRQn), unlike CM0PLUS's
     * combined RTC_LSECSS line. STM32duino builds the CM4 core. */
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
}

static void enter_stop2_seconds(uint32_t s)
{
    if (s < 1) s = 1;
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (uint32_t)(s - 1),
                                RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0);
    HAL_SuspendTick();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);   /* sleeps here */
    HAL_ResumeTick();                              /* woken by RTC */
}

/* RTC wakeup timer ISR (CM4 dedicated line). C linkage to match the vector. */
extern "C" void RTC_WKUP_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}

/* -------------------------------------------------------------------------- */
void setup(void)
{
    DS_PROBE_GPIO_CLK();

    rtc_clock_init();
    rtc_init();
    gate_pin_init();
    sensor_pins_park();
}

void loop(void)
{
#if defined(DEBUG_UART_ENABLED)
    Serial.begin(DEBUG_UART_BAUD);
#endif
    dbgf("\r\n[node %u] wake\r\n", (unsigned)NODE_ID);

    /* --- power the whole sensor rail --- */
    gate_on();
    delay(DS_POWER_SETTLE_MS);

    /* Kick off EVERY 1-Wire conversion FIRST. The old 1 s SCD41 power-up wait
     * that used to be overlapped here is gone with the part, so rail-on time is
     * now just the 750 ms conversion.
     *
     * Each probe has its own pin, so the conversions overlap: six probes cost
     * one 750 ms wait, not six. Only the bit-banged traffic scales (~7 ms per
     * probe), and that hides inside this same wait. */
    int16_t temp[DS_PROBE_COUNT];
    int     present[DS_PROBE_COUNT];
    int     ok[DS_PROBE_COUNT];

    for (int i = 0; i < DS_PROBE_COUNT; i++) {
        ds18b20_init(&probe_bus[i]);
        present[i] = ds18b20_start_convert(&probe_bus[i]);
        temp[i]    = 0;
        ok[i]      = 0;
    }
    delay(I2C_POWER_SETTLE_MS - DS_POWER_SETTLE_MS);

    for (int i = 0; i < DS_PROBE_COUNT; i++) {
        ok[i] = present[i] && ds18b20_read(&probe_bus[i], &temp[i]);
    }

    /* --- SHT45 x3 (air temperature + humidity) ---
     * All three answer to 0x44, so exactly one is connected to the bus at any
     * moment. sht45_init() is re-run per sensor on purpose: it soft-resets and
     * does a throwaway read, which is how we find out a given channel is
     * actually populated rather than trusting the config. */
    i2c_sens.begin();
    i2c_sens.setClock(I2C_CLOCK_HZ);

    int16_t  air[SHT45_COUNT];
    uint16_t hum[SHT45_COUNT];
    int      oksh[SHT45_COUNT];

    for (int i = 0; i < SHT45_COUNT; i++) {
        air[i]  = 0;
        hum[i]  = 0;
        oksh[i] = sht45_select(i)
                  && sht45_init(&i2c_sens, SHT45_ADDR)
                  && sht45_read(&air[i], &hum[i]);
    }
    /* Leave nothing downstream bridged on when we let go of the bus. */
    sht45_select(-1);

    i2c_sens.end();
    gate_off();
    sensor_pins_park();

    /* --- CO2 (S88), AFTER the gate closes ---
     * Deliberately outside the gated section: the S88 has its own always-on
     * 5 V rail and its own UART, so it shares nothing with the I2C parts.
     * Reading it here makes that independence obvious, and keeps the gated rail
     * on for the shortest possible time. */
    if (!co2_site_ok)
        co2_site_ok = (uint8_t)s88_apply_site_pressure(S88_SITE_PRESSURE_DHPA);
    uint16_t co2        = 0;
    uint16_t co2_status = 0xFFFF;     /* 0xFFFF = no valid frame at all */
    int      okco2      = s88_read_co2(&co2, &co2_status);
    const int co2_fresh = okco2;      /* for the log line, below */
    if (okco2) {
        co2_last  = co2;
        co2_valid = 1;
    } else if (co2_valid) {
        /* Modbus CRC caught a bad frame, or the sensor did not answer. Re-send
         * the last good value rather than dropping the metric — it is real, just
         * one wake stale. */
        co2   = co2_last;
        okco2 = 1;
    }

    /* --- battery --- */
    uint16_t vmv = battery_read_mv();

    /* --- build the frame (v4: 6 water temps + battery + SHT45 + CO2) ---
     * Probes 0/1 keep the original two temperature slots (and their HOT/COLD
     * flags) so an existing dashboard's history is unbroken; probes 2..5 carry
     * their own validity as the LORA_TEMP_INVALID sentinel, because `flags` has
     * no spare bits. A probe that is not fitted, or failed to answer, is
     * transmitted as that sentinel and dropped by the gateway. */
    lora_payload_t p;
    p.node_id        = NODE_ID;
    p.seq            = seq++;
    p.flags          = 0;
    p.battery_mv     = vmv;
    p.pressure_dhpa  = 0;      /* no barometer on this node */
    p.co2_ppm        = co2;

    for (int i = 0; i < LORA_PROBE_MAX; i++) {
        p.probe_c100[i] = (i < DS_PROBE_COUNT && ok[i]) ? temp[i]
                                                        : LORA_TEMP_INVALID;
    }
    for (int i = 0; i < LORA_AIR_MAX; i++) {
        int good = (i < SHT45_COUNT) && oksh[i];
        p.air_temp_c100[i] = good ? air[i] : LORA_TEMP_INVALID;
        p.humidity_x100[i] = good ? hum[i] : LORA_HUM_INVALID;
    }
    if (ok[0]) p.flags |= LORA_FLAG_HOT;
    if (ok[1]) p.flags |= LORA_FLAG_COLD;
    if (vmv)   p.flags |= LORA_FLAG_BATT;
    /* Sensor 0 keeps the v2 flag bits; sensors 1/2 carry validity as their
     * sentinel, because `flags` is full. LORA_FLAG_SHT is set whenever any of
     * them answered -- it describes the part type, not a particular sensor. */
    if (oksh[0]) p.flags |= (LORA_FLAG_AIR | LORA_FLAG_HUM);
    if (oksh[0] || oksh[1] || oksh[2]) p.flags |= LORA_FLAG_SHT;
    if (okco2) p.flags |= LORA_FLAG_CO2;

    uint8_t buf[LORA_PKT_LEN_V5];
    lora_packet_pack_v5(&p, buf);

    /* --- transmit --- */
    if (subghz_lora_init() == 0) {
        int rc = subghz_lora_send(buf, LORA_PKT_LEN_V5, LORA_TX_TIMEOUT_MS);
        dbgf("tx rc=%d  batt=%umV seq=%u\r\n", rc, (unsigned)vmv, (unsigned)p.seq);
        for (int i = 0; i < DS_PROBE_COUNT; i++) {
            dbgf("         p%d=%d/%d\r\n", i, (int)temp[i], ok[i]);
        }
        for (int i = 0; i < SHT45_COUNT; i++) {
            dbgf("         air%d=%d/%d rh%d=%u\r\n",
                 i, (int)air[i], oksh[i], i, (unsigned)hum[i]);
        }
        dbgf("         co2=%u/%d st=0x%04X%s\r\n", (unsigned)co2, okco2,
             (unsigned)co2_status,
             co2_fresh ? "" : (co2_status == 0xFFFF
                                   ? " (cached - S88 did not answer)"
                                   : " (cached - S88 status not OK)"));
        subghz_lora_sleep();
    } else {
        dbgf("radio init FAILED\r\n");
    }

#if defined(DEBUG_UART_ENABLED)
    Serial.flush();
    Serial.end();
#endif

    enter_stop2_seconds(WAKE_INTERVAL_S);
    SystemClock_Config();   /* Stop2 dropped us to MSI — restore the core clock */
}
