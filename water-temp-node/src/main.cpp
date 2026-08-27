/*
 * water-temp-node — main.cpp  (STM32duino / Arduino framework)
 *
 * NUCLEO-WL55JC1 battery sensor node. Every WAKE_INTERVAL_S it wakes from Stop2,
 * powers the sensor rail through the A0341 P-MOSFET, reads hot+cold water
 * temperature (2x DS18B20), air temperature + humidity (SHT45) and CO2 (SCD41),
 * measures the battery via the internal reference, transmits ONE compact LoRa
 * uplink (AS923), then sleeps. Uplink-only — it never listens.
 *
 * All four sensors sit on the ONE gated rail, so the whole front-end is off
 * between wakes. The SCD41 is the expensive one — see CO2_EVERY_N_WAKES in
 * node_config.h and README, "CO2 on a battery node".
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
#include "scd41.h"
#include "battery.h"
#include "node_config.h"

/* Provided (weak) by the STM32duino variant; we call it to restore the clock
 * tree after Stop2, which drops the core back to MSI. */
extern "C" void SystemClock_Config(void);

static RTC_HandleTypeDef hrtc;

static const ds_bus_t bus_hot  = { DS_HOT_PORT,  DS_HOT_PIN  };
static const ds_bus_t bus_cold = { DS_COLD_PORT, DS_COLD_PIN };
static uint8_t seq = 0;

/* SHT45 + SCD41 share this bus, and share the probes' gated rail. */
static TwoWire i2c_sens(I2C_SDA_PIN, I2C_SCL_PIN);

/*
 * CO2 is measured only every CO2_EVERY_N_WAKES-th wake (the SCD41 dominates the
 * energy budget), so the last good reading is cached here and re-sent in
 * between. RAM survives Stop2, so these statics persist across wakes.
 */
static uint8_t  wake_count = 0;
static uint16_t co2_last   = 0;
static uint8_t  co2_valid  = 0;

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
/* Sensor-rail power gate (A0341 P-MOSFET, active-low). */
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
    g.Pin  = DS_HOT_PIN;  HAL_GPIO_Init(DS_HOT_PORT,  &g);
    g.Pin  = DS_COLD_PIN; HAL_GPIO_Init(DS_COLD_PORT, &g);

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
    DS_HOT_GPIO_CLK();
    DS_COLD_GPIO_CLK();

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

    /* Is this a CO2 wake? Decided up front so the debug line can say so. */
#if defined(CO2_ENABLED)
    int co2_due = (wake_count % CO2_EVERY_N_WAKES) == 0;
#else
    int co2_due = 0;
#endif
    wake_count++;

    /* --- power the whole sensor rail --- */
    gate_on();
    delay(DS_POWER_SETTLE_MS);

    /* Kick off both 1-Wire conversions FIRST, then spend the SCD41's mandatory
     * 1 s power-up wait covering the DS18B20's 750 ms conversion instead of
     * adding to it. The rail is on for the longer of the two, not the sum. */
    ds18b20_init(&bus_hot);
    ds18b20_init(&bus_cold);
    int ph = ds18b20_start_convert(&bus_hot);
    int pc = ds18b20_start_convert(&bus_cold);
    delay(I2C_POWER_SETTLE_MS - DS_POWER_SETTLE_MS);

    int16_t th = 0, tc = 0;
    int okh = ph && ds18b20_read(&bus_hot,  &th);
    int okc = pc && ds18b20_read(&bus_cold, &tc);

    /* --- SHT45 (air temperature + humidity) --- */
    i2c_sens.begin();
    i2c_sens.setClock(I2C_CLOCK_HZ);

    int16_t  air  = 0;
    uint16_t hum  = 0;
    int      oksh = sht45_init(&i2c_sens, SHT45_ADDR) && sht45_read(&air, &hum);

    /* --- SCD41 (CO2), only on a CO2 wake --- */
    uint16_t co2   = 0;
    int      okco2 = 0;
    if (co2_due) {
        if (scd41_init(&i2c_sens, SCD41_ADDR)) {
            okco2 = scd41_read_single_shot(CO2_SINGLE_SHOT_WARMUP, &co2, 0, 0);
        }
        if (okco2) {
            co2_last  = co2;
            co2_valid = 1;
        }
    }
    /* On a non-CO2 wake (or a failed read) re-send the cached value, if we ever
     * got one. The flag still says "valid" because the number is real — just up
     * to CO2_EVERY_N_WAKES intervals old, which a CO2 trend can live with. */
    if (!okco2 && co2_valid) {
        co2   = co2_last;
        okco2 = 1;
    }

    i2c_sens.end();
    gate_off();
    sensor_pins_park();

    /* --- battery --- */
    uint16_t vmv = battery_read_mv();

    /* --- build the frame (v3: water temps + battery + SHT45 + CO2) --- */
    lora_payload_t p;
    p.node_id        = NODE_ID;
    p.seq            = seq++;
    p.flags          = 0;
    p.temp_hot_c100  = okh ? th : LORA_TEMP_INVALID;
    p.temp_cold_c100 = okc ? tc : LORA_TEMP_INVALID;
    p.battery_mv     = vmv;
    p.air_temp_c100  = air;
    p.humidity_x100  = hum;
    p.pressure_dhpa  = 0;      /* no barometer on this node */
    p.co2_ppm        = co2;
    if (okh)   p.flags |= LORA_FLAG_HOT;
    if (okc)   p.flags |= LORA_FLAG_COLD;
    if (vmv)   p.flags |= LORA_FLAG_BATT;
    if (oksh)  p.flags |= (LORA_FLAG_AIR | LORA_FLAG_HUM | LORA_FLAG_SHT);
    if (okco2) p.flags |= LORA_FLAG_CO2;

    uint8_t buf[LORA_PKT_LEN_V3];
    lora_packet_pack_v3(&p, buf);

    /* --- transmit --- */
    if (subghz_lora_init() == 0) {
        int rc = subghz_lora_send(buf, LORA_PKT_LEN_V3, LORA_TX_TIMEOUT_MS);
        dbgf("tx rc=%d  hot=%d/%d cold=%d/%d batt=%umV seq=%u\r\n",
             rc, (int)th, okh, (int)tc, okc, (unsigned)vmv, (unsigned)p.seq);
        dbgf("         air=%d/%d rh=%u co2=%u/%d%s\r\n",
             (int)air, oksh, (unsigned)hum, (unsigned)co2, okco2,
             co2_due ? "" : " (cached)");
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
