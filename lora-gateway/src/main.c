/*
 * lora-gateway — main.c
 *
 * NUCLEO-WL55JC1 receive-only LoRa gateway. Listens continuously on the shared
 * AS923 channel, validates each frame's CRC, maps node_id -> device_id, and
 * prints ONE JSON line per frame out LPUART1 (the ST-LINK virtual COM port). A
 * host-side bridge (lora-gateway/bridge/) reads those lines and POSTs them to
 * the web-server's /api/v1/telemetry.
 *
 *   node (LoRa) --> [this gateway] --USB CDC JSON--> bridge.js --HTTP--> :3000
 *
 * Line format (rssi/snr added here from the LoRa RX):
 *   {"device_id":"water-temp-01","metrics":{"temp_hot":41.30,"temp_cold":22.60,
 *    "battery_v":3.140,"rssi":-92,"snr":8.5}}
 * Non-JSON lines (starting with '#') are diagnostics the bridge ignores.
 */
#include <string.h>
#include <stdio.h>

#include "stm32wlxx_hal.h"
#include "board.h"
#include "gateway_config.h"
#include "lora/subghz_lora.h"
#include "lora/lora_packet.h"

static UART_HandleTypeDef hlpuart;

/* -------------------------------------------------------------------------- */
static void uart_init(void)
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

    hlpuart.Instance            = LPUART1;
    hlpuart.Init.BaudRate       = GW_UART_BAUD;
    hlpuart.Init.WordLength     = UART_WORDLENGTH_8B;
    hlpuart.Init.StopBits       = UART_STOPBITS_1;
    hlpuart.Init.Parity         = UART_PARITY_NONE;
    hlpuart.Init.Mode           = UART_MODE_TX_RX;
    hlpuart.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
    hlpuart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    hlpuart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&hlpuart) != HAL_OK) {
        Error_Handler();
    }
}

static void uart_puts(const char *s)
{
    HAL_UART_Transmit(&hlpuart, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void led_init(void)
{
    GW_RX_LED_CLK();
    GPIO_InitTypeDef g = {0};
    g.Pin   = GW_RX_LED_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GW_RX_LED_PORT, &g);
    HAL_GPIO_WritePin(GW_RX_LED_PORT, GW_RX_LED_PIN, GPIO_PIN_RESET);
}
static void led_blink(void)
{
    HAL_GPIO_WritePin(GW_RX_LED_PORT, GW_RX_LED_PIN, GPIO_PIN_SET);
    HAL_Delay(15);
    HAL_GPIO_WritePin(GW_RX_LED_PORT, GW_RX_LED_PIN, GPIO_PIN_RESET);
}

/* -------------------------------------------------------------------------- */
/* Append "<int>.<NN>" for a centi value (e.g. -412 -> "-4.12"), no float printf. */
static int append_centi(char *dst, int cap, int centi)
{
    int neg = centi < 0;
    int a = neg ? -centi : centi;
    return snprintf(dst, cap, "%s%d.%02d", neg ? "-" : "", a / 100, a % 100);
}

/* Build the telemetry JSON line into out. Returns strlen. */
static int build_json(char *out, int cap, const lora_payload_t *p,
                      int rssi, int snr_tenths)
{
    char idbuf[24];
    const char *id = gw_device_id(p->node_id);
    if (id == 0) {
        snprintf(idbuf, sizeof(idbuf), "water-node-%u", (unsigned)p->node_id);
        id = idbuf;
    }

    int n = 0;
    n += snprintf(out + n, cap - n, "{\"device_id\":\"%s\",\"metrics\":{", id);

    int first = 1;
    if (p->flags & LORA_FLAG_HOT) {
        n += snprintf(out + n, cap - n, "%s\"temp_hot\":", first ? "" : ",");
        n += append_centi(out + n, cap - n, p->temp_hot_c100);
        first = 0;
    }
    if (p->flags & LORA_FLAG_COLD) {
        n += snprintf(out + n, cap - n, "%s\"temp_cold\":", first ? "" : ",");
        n += append_centi(out + n, cap - n, p->temp_cold_c100);
        first = 0;
    }
    if (p->flags & LORA_FLAG_BATT) {
        n += snprintf(out + n, cap - n, "%s\"battery_v\":%u.%03u",
                      first ? "" : ",",
                      (unsigned)(p->battery_mv / 1000),
                      (unsigned)(p->battery_mv % 1000));
        first = 0;
    }
    /* Link quality + seq always included (gateway-observed). */
    int sneg = snr_tenths < 0;
    int sa = sneg ? -snr_tenths : snr_tenths;
    n += snprintf(out + n, cap - n,
                  "%s\"rssi\":%d,\"snr\":%s%d.%d,\"seq\":%u}}\r\n",
                  first ? "" : ",", rssi,
                  sneg ? "-" : "", sa / 10, sa % 10, (unsigned)p->seq);
    return n;
}

/* -------------------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    DWT_Delay_Init();

    uart_init();
    led_init();

    uart_puts("# lora-gateway up: AS923 923.2MHz SF9BW125, RX-only\r\n");

    if (subghz_lora_init() != 0) {
        uart_puts("# radio init FAILED\r\n");
        Error_Handler();
    }
    subghz_lora_recv_start();
    uart_puts("# listening\r\n");

    uint8_t  buf[LORA_PKT_LEN + 4];
    char     line[192];

    for (;;) {
        uint8_t  len = 0;
        int      rssi = 0;
        float    snr = 0.0f;

        int r = subghz_lora_recv_poll(buf, sizeof(buf), &len, &rssi, &snr);
        if (r == 1) {
            lora_payload_t p;
            if (lora_packet_unpack(buf, len, &p)) {
                int snr_tenths = (int)(snr * 10.0f + (snr >= 0 ? 0.5f : -0.5f));
                int n = build_json(line, sizeof(line), &p, rssi, snr_tenths);
                if (n > 0) {
                    HAL_UART_Transmit(&hlpuart, (uint8_t *)line,
                                      (uint16_t)(n < (int)sizeof(line) ? n : (int)sizeof(line)), 200);
                    led_blink();
                }
            }
            /* CRC/magic already validated in unpack; bad frames silently dropped. */
        } else if (r < 0) {
            /* transient radio error — re-arm continuous RX */
            subghz_lora_recv_start();
        }
        HAL_Delay(2);
    }
}
