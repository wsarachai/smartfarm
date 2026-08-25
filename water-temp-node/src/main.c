/*
 * water-temp-node — main.c
 *
 * NUCLEO-WL55JC1 battery sensor node. Every WAKE_INTERVAL_S it wakes from Stop2,
 * powers two DS18B20 probes through the A0341 P-MOSFET, reads hot+cold water
 * temperature, measures the battery via VREFINT, transmits ONE compact LoRa
 * uplink (AS923), then sleeps. Uplink-only — it never listens.
 *
 *   loop: RTC wake -> gate ON -> settle -> convert 750ms -> read -> gate OFF
 *         -> read battery -> pack -> LoRa TX -> radio sleep -> Stop2
 *
 * See lora-gateway/ for the receiver and CLAUDE.md for the topology.
 */
#include <stdarg.h>
#include <stdio.h>

#include "stm32wlxx_hal.h"
#include "node_config.h"
#include "board.h"
#include "ds18b20.h"
#include "battery.h"
#include "lora/subghz_lora.h"
#include "lora/lora_packet.h"

static RTC_HandleTypeDef  hrtc;
#ifdef DEBUG_UART_ENABLED
static UART_HandleTypeDef hlpuart;
#endif

static const ds_bus_t bus_hot  = { DS_HOT_PORT,  DS_HOT_PIN  };
static const ds_bus_t bus_cold = { DS_COLD_PORT, DS_COLD_PIN };

/* -------------------------------------------------------------------------- */
/* Debug log over LPUART1 (ST-LINK VCP, PA2/PA3). No-op if DEBUG_UART disabled. */
#ifdef DEBUG_UART_ENABLED
static void dbg_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_LPUART1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;   /* PA2=TX, PA3=RX */
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF8_LPUART1;
    HAL_GPIO_Init(GPIOA, &g);

    hlpuart.Instance                    = LPUART1;
    hlpuart.Init.BaudRate               = DEBUG_UART_BAUD;
    hlpuart.Init.WordLength             = UART_WORDLENGTH_8B;
    hlpuart.Init.StopBits               = UART_STOPBITS_1;
    hlpuart.Init.Parity                 = UART_PARITY_NONE;
    hlpuart.Init.Mode                   = UART_MODE_TX_RX;
    hlpuart.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    hlpuart.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    hlpuart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&hlpuart);
}

static void dbg_deinit(void)
{
    HAL_UART_DeInit(&hlpuart);
    /* Park the pins as analog so they don't sink current in Stop2. */
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
    __HAL_RCC_LPUART1_CLK_DISABLE();
}

static void dbg(const char *fmt, ...)
{
    char line[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n > 0) {
        HAL_UART_Transmit(&hlpuart, (uint8_t *)line,
                          (uint16_t)(n < (int)sizeof(line) ? n : (int)sizeof(line)), 100);
    }
}
#else
static void dbg_init(void)   {}
static void dbg_deinit(void) {}
static void dbg(const char *fmt, ...) { (void)fmt; }
#endif

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

/* Park the 1-Wire pins as analog while the rail is unpowered (no back-feed). */
static void ds_pins_park(void)
{
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin  = DS_HOT_PIN;  HAL_GPIO_Init(DS_HOT_PORT,  &g);
    g.Pin  = DS_COLD_PIN; HAL_GPIO_Init(DS_COLD_PORT, &g);
}

/* -------------------------------------------------------------------------- */
static void rtc_init(void)
{
    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();

    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;      /* LSE 32768 / (127+1) / (255+1) = 1 Hz */
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    hrtc.Init.OutPutRemap    = RTC_OUTPUT_REMAP_NONE;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(RTC_LSECSS_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(RTC_LSECSS_IRQn);
}

static void enter_stop2_seconds(uint32_t s)
{
    if (s < 1) s = 1;
    /* ck_spre (1 Hz) wakeup clock; counter counts s ticks. */
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (uint32_t)(s - 1),
                                    RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0) != HAL_OK) {
        Error_Handler();
    }

    HAL_SuspendTick();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);   /* <-- sleeps here */
    HAL_ResumeTick();                              /* woken by RTC    */
}

/* RTC wakeup timer ISR (also serves alarms/tamper — combined line on WL). */
void RTC_LSECSS_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *h) { (void)h; }

/* -------------------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    DWT_Delay_Init();
    rtc_init();
    gate_pin_init();
    ds_pins_park();

    uint8_t seq = 0;

    for (;;) {
        DWT_Delay_Init();   /* re-arm the cycle counter (Stop2 may clear it) */
        dbg_init();
        dbg("\r\n[node %u] wake\r\n", (unsigned)NODE_ID);

        /* --- power the probes and read hot + cold --- */
        gate_on();
        HAL_Delay(DS_POWER_SETTLE_MS);
        ds18b20_init(&bus_hot);
        ds18b20_init(&bus_cold);
        int ph = ds18b20_start_convert(&bus_hot);
        int pc = ds18b20_start_convert(&bus_cold);
        HAL_Delay(DS_CONVERT_MS);

        int16_t th = 0, tc = 0;
        int okh = ph && ds18b20_read(&bus_hot,  &th);
        int okc = pc && ds18b20_read(&bus_cold, &tc);

        gate_off();
        ds_pins_park();

        /* --- battery --- */
        uint16_t vmv = battery_read_mv();

        /* --- build the frame --- */
        lora_payload_t p = {
            .node_id        = NODE_ID,
            .seq            = seq++,
            .flags          = 0,
            .temp_hot_c100  = okh ? th : LORA_TEMP_INVALID,
            .temp_cold_c100 = okc ? tc : LORA_TEMP_INVALID,
            .battery_mv     = vmv,
        };
        if (okh) p.flags |= LORA_FLAG_HOT;
        if (okc) p.flags |= LORA_FLAG_COLD;
        if (vmv) p.flags |= LORA_FLAG_BATT;

        uint8_t buf[LORA_PKT_LEN];
        lora_packet_pack(&p, buf);

        /* --- transmit --- */
        if (subghz_lora_init() == 0) {
            int rc = subghz_lora_send(buf, LORA_PKT_LEN, LORA_TX_TIMEOUT_MS);
            dbg("tx rc=%d  hot=%d/%d cold=%d/%d batt=%umV seq=%u\r\n",
                rc, (int)th, okh, (int)tc, okc, (unsigned)vmv, (unsigned)p.seq);
            subghz_lora_sleep();
        } else {
            dbg("radio init FAILED\r\n");
        }

        dbg_deinit();

        enter_stop2_seconds(WAKE_INTERVAL_S);
        SystemClock_Config();   /* Stop2 drops us back to MSI — restore the tree */
    }
}
