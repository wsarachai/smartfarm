/*
 * subghz_lora.c — see subghz_lora.h. Raw-LoRa over STM32WL SubGHz via HAL_SUBGHZ.
 * SHARED file: keep identical in water-temp-node and lora-gateway.
 */
#include "subghz_lora.h"
#include "lora_params.h"

#include "stm32wlxx_hal.h"

/* ---- SX126x command opcodes (names match ST's SUBGHZ_RadioSetCmd_t) -------- */
#define OP_SET_SLEEP            0x84
#define OP_SET_STANDBY         0x80
#define OP_SET_TX               0x83
#define OP_SET_RX               0x82
#define OP_SET_PACKETTYPE       0x8A
#define OP_SET_RFFREQ           0x86
#define OP_SET_TXPARAMS         0x8E
#define OP_SET_PACONFIG         0x95
#define OP_SET_BUFBASE          0x8F
#define OP_SET_MODPARAMS        0x8B
#define OP_SET_PKTPARAMS        0x8C
#define OP_SET_REGULATOR        0x96
#define OP_SET_TCXO             0x97
#define OP_CALIBRATE            0x89
#define OP_CFG_DIOIRQ           0x08
#define OP_CLR_IRQ              0x02
#define OP_GET_IRQ              0x12
#define OP_GET_RXBUFSTATUS      0x13
#define OP_GET_PKTSTATUS        0x14

/* STANDBY modes */
#define STDBY_RC                0x00
/* Packet type */
#define PKT_TYPE_LORA           0x01
/* Regulator mode: DC-DC + LDO */
#define REG_MODE_DCDC           0x01
/* Ramp time 200 us */
#define RAMP_200US              0x04

/* IRQ bit masks */
#define IRQ_TX_DONE             0x0001
#define IRQ_RX_DONE             0x0002
#define IRQ_CRC_ERR             0x0040
#define IRQ_TIMEOUT             0x0200

/* LoRa sync word register (SX126x) */
#define REG_LORA_SYNCWORD_MSB   0x0740
#define REG_LORA_SYNCWORD_LSB   0x0741

/* VREFINT/factory cal not used here; radio XTAL frequency for freq math. */
#define RADIO_XTAL_HZ           32000000UL

static SUBGHZ_HandleTypeDef hsubghz;

/* HAL_SUBGHZ_Init() calls this to bring up the SUBGHZSPI peripheral clock.
 * Weak in the HAL; we override it here (shared by node + gateway).
 *
 * We deliberately do NOT enable the SUBGHZ_Radio_IRQn NVIC line: this driver
 * POLLS GetIrqStatus, and there is no ISR. The radio still latches TxDone/RxDone
 * into its IRQ-status register regardless of the NVIC. Enabling the line without
 * a handler would vector the first RxDone/TxDone into the startup file's default
 * infinite-loop handler and hang. */
void HAL_SUBGHZ_MspInit(SUBGHZ_HandleTypeDef *h)
{
    (void)h;
    __HAL_RCC_SUBGHZSPI_CLK_ENABLE();
}

void HAL_SUBGHZ_MspDeInit(SUBGHZ_HandleTypeDef *h)
{
    (void)h;
    __HAL_RCC_SUBGHZSPI_CLK_DISABLE();
}

/* -------------------------------------------------------------------------- */
/* RF switch (NUCLEO-WL55JC1): FE_CTRL1=PC4, FE_CTRL2=PC5, FE_CTRL3=PC3.
 * Truth table copied verbatim from ST BSP BSP_RADIO_ConfigRFSwitch:
 *   OFF    C1=0 C2=0 C3=0
 *   RX     C1=1 C2=0 C3=1
 *   RFO_LP C1=1 C2=1 C3=1   (TX low-power PA, used for +14 dBm here)
 */
#define RF_C1_PORT GPIOC
#define RF_C1_PIN  GPIO_PIN_4
#define RF_C2_PORT GPIOC
#define RF_C2_PIN  GPIO_PIN_5
#define RF_C3_PORT GPIOC
#define RF_C3_PIN  GPIO_PIN_3

typedef enum { RF_OFF, RF_RX, RF_TX_LP } rf_mode_t;

static void rf_switch_gpio_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Pin   = RF_C1_PIN; HAL_GPIO_Init(RF_C1_PORT, &g);
    g.Pin   = RF_C2_PIN; HAL_GPIO_Init(RF_C2_PORT, &g);
    g.Pin   = RF_C3_PIN; HAL_GPIO_Init(RF_C3_PORT, &g);
}

static void rf_switch_set(rf_mode_t m)
{
    GPIO_PinState c1 = GPIO_PIN_RESET, c2 = GPIO_PIN_RESET, c3 = GPIO_PIN_RESET;
    switch (m) {
        case RF_RX:    c1 = GPIO_PIN_SET; c2 = GPIO_PIN_RESET; c3 = GPIO_PIN_SET; break;
        case RF_TX_LP: c1 = GPIO_PIN_SET; c2 = GPIO_PIN_SET;   c3 = GPIO_PIN_SET; break;
        case RF_OFF:   default: break;
    }
    HAL_GPIO_WritePin(RF_C1_PORT, RF_C1_PIN, c1);
    HAL_GPIO_WritePin(RF_C2_PORT, RF_C2_PIN, c2);
    HAL_GPIO_WritePin(RF_C3_PORT, RF_C3_PIN, c3);
}

/* -------------------------------------------------------------------------- */
static int cmd(uint8_t op, uint8_t *buf, uint16_t n)
{
    return (HAL_SUBGHZ_ExecSetCmd(&hsubghz, (SUBGHZ_RadioSetCmd_t)op, buf, n)
            == HAL_OK) ? 0 : -1;
}
static int getcmd(uint8_t op, uint8_t *buf, uint16_t n)
{
    return (HAL_SUBGHZ_ExecGetCmd(&hsubghz, (SUBGHZ_RadioGetCmd_t)op, buf, n)
            == HAL_OK) ? 0 : -1;
}

static void set_irq(uint16_t mask)
{
    /* irqMask + dio1Mask=mask, dio2/dio3=0 (we poll GetIrqStatus, DIO routing
     * is irrelevant but the radio still latches into IRQ status). */
    uint8_t b[8] = {
        (uint8_t)(mask >> 8), (uint8_t)mask,   /* global IRQ mask   */
        (uint8_t)(mask >> 8), (uint8_t)mask,   /* DIO1              */
        0, 0,                                  /* DIO2              */
        0, 0                                   /* DIO3              */
    };
    cmd(OP_CFG_DIOIRQ, b, 8);
}
static uint16_t get_irq(void)
{
    uint8_t b[2] = {0, 0};
    getcmd(OP_GET_IRQ, b, 2);
    return (uint16_t)((b[0] << 8) | b[1]);
}
static void clear_irq(uint16_t mask)
{
    uint8_t b[2] = { (uint8_t)(mask >> 8), (uint8_t)mask };
    cmd(OP_CLR_IRQ, b, 2);
}

static void set_packet_len(uint8_t len)
{
    uint8_t b[6] = {
        (uint8_t)(LORA_PREAMBLE_SYMS >> 8), (uint8_t)LORA_PREAMBLE_SYMS,
        0x00,   /* explicit header */
        len,
        0x01,   /* CRC on          */
        0x00    /* standard IQ     */
    };
    cmd(OP_SET_PKTPARAMS, b, 6);
}

/* -------------------------------------------------------------------------- */
int subghz_lora_init(void)
{
    rf_switch_gpio_init();
    rf_switch_set(RF_OFF);

    /* Force a full bring-up (MspInit + radio reset) even on a second call after
     * the node returns from Stop2 with the static handle still marked READY. */
    hsubghz.State = HAL_SUBGHZ_STATE_RESET;
    hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_8;
    if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK) {
        return -1;
    }

    uint8_t v;

    v = STDBY_RC;        cmd(OP_SET_STANDBY, &v, 1);
    v = REG_MODE_DCDC;   cmd(OP_SET_REGULATOR, &v, 1);

    /* TCXO on DIO3 @ 1.7 V, 5 ms startup (timeout in 15.625 us steps: 320). */
    {
        uint8_t b[4] = { 0x01, 0x00, 0x01, 0x40 };   /* 1.7V, 0x000140 = 320 */
        cmd(OP_SET_TCXO, b, 4);
    }
    /* After switching to TCXO the datasheet requires a full calibration. */
    v = 0x7F;            cmd(OP_CALIBRATE, &v, 1);

    v = PKT_TYPE_LORA;   cmd(OP_SET_PACKETTYPE, &v, 1);

    /* RF frequency: freq = F_RF * 2^25 / F_XTAL */
    {
        uint32_t frf = (uint32_t)(((uint64_t)LORA_FREQ_HZ << 25) / RADIO_XTAL_HZ);
        uint8_t b[4] = { (uint8_t)(frf >> 24), (uint8_t)(frf >> 16),
                         (uint8_t)(frf >> 8),  (uint8_t)frf };
        cmd(OP_SET_RFFREQ, b, 4);
    }

    /* Buffer base addresses: TX=0x00, RX=0x00 */
    {
        uint8_t b[2] = { 0x00, 0x00 };
        cmd(OP_SET_BUFBASE, b, 2);
    }

    /* Modulation: SF, BW, CR, LDRO */
    {
        uint8_t b[4] = { LORA_SF, LORA_BW_CODE, LORA_CR_CODE, LORA_LDRO };
        cmd(OP_SET_MODPARAMS, b, 4);
    }

    /* Packet params (length) are set per-operation in send()/recv_start().    */

    /* LoRa sync word */
    {
        uint8_t msb = LORA_SYNCWORD_MSB, lsb = LORA_SYNCWORD_LSB;
        HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_LORA_SYNCWORD_MSB, &msb, 1);
        HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_LORA_SYNCWORD_LSB, &lsb, 1);
    }

    clear_irq(0xFFFF);
    return 0;
}

int subghz_lora_send(const uint8_t *buf, uint8_t len, uint32_t timeout_ms)
{
    /* PA config for the low-power PA (+14 dBm well within LP's 15 dBm max). */
    {
        uint8_t b[4] = { 0x04, 0x00, 0x01, 0x01 };   /* paDuty, hpMax, devSel=LP, paLut */
        cmd(OP_SET_PACONFIG, b, 4);
    }
    {
        uint8_t b[2] = { (uint8_t)LORA_TX_POWER_DBM, RAMP_200US };
        cmd(OP_SET_TXPARAMS, b, 2);
    }

    set_packet_len(len);
    if (HAL_SUBGHZ_WriteBuffer(&hsubghz, 0x00, (uint8_t *)buf, len) != HAL_OK) {
        return -1;
    }

    set_irq(IRQ_TX_DONE | IRQ_TIMEOUT);
    clear_irq(0xFFFF);

    rf_switch_set(RF_TX_LP);
    {
        /* SetTx with radio timeout disabled (0); we bound it in software. */
        uint8_t b[3] = { 0x00, 0x00, 0x00 };
        cmd(OP_SET_TX, b, 3);
    }

    uint32_t t0 = HAL_GetTick();
    int rc = -1;
    for (;;) {
        uint16_t irq = get_irq();
        if (irq & IRQ_TX_DONE) { rc = 0; break; }
        if (irq & IRQ_TIMEOUT) { rc = -2; break; }
        if ((HAL_GetTick() - t0) > timeout_ms) { rc = -3; break; }
    }
    clear_irq(0xFFFF);
    rf_switch_set(RF_OFF);
    return rc;
}

void subghz_lora_sleep(void)
{
    rf_switch_set(RF_OFF);
    /* Cold sleep (config discarded) — lowest current. We fully reconfigure on
     * the next subghz_lora_init() after the node wakes from Stop2. */
    uint8_t v = 0x00;
    cmd(OP_SET_SLEEP, &v, 1);
}

int subghz_lora_recv_start(void)
{
    set_packet_len(255);   /* max, explicit header carries the real length */
    set_irq(IRQ_RX_DONE | IRQ_CRC_ERR | IRQ_TIMEOUT);
    clear_irq(0xFFFF);
    rf_switch_set(RF_RX);
    {
        /* SetRx continuous (timeout = 0xFFFFFF). */
        uint8_t b[3] = { 0xFF, 0xFF, 0xFF };
        cmd(OP_SET_RX, b, 3);
    }
    return 0;
}

int subghz_lora_recv_poll(uint8_t *buf, uint8_t max_len, uint8_t *out_len,
                          int *rssi_dbm, float *snr_db)
{
    uint16_t irq = get_irq();
    if (irq == 0) {
        return 0;
    }
    if (irq & IRQ_CRC_ERR) {
        clear_irq(0xFFFF);
        return 0;   /* corrupt frame — drop, stay in continuous RX */
    }
    if (!(irq & IRQ_RX_DONE)) {
        clear_irq(0xFFFF);
        return 0;
    }

    /* RxBufferStatus: [payloadLen][rxStartPtr] */
    uint8_t st[2] = {0, 0};
    getcmd(OP_GET_RXBUFSTATUS, st, 2);
    uint8_t plen = st[0];
    uint8_t ptr  = st[1];
    if (plen > max_len) plen = max_len;

    if (HAL_SUBGHZ_ReadBuffer(&hsubghz, ptr, buf, plen) != HAL_OK) {
        clear_irq(0xFFFF);
        return -1;
    }
    *out_len = plen;

    /* PacketStatus (LoRa): [RssiPkt][SnrPkt][SignalRssiPkt] */
    uint8_t ps[3] = {0, 0, 0};
    getcmd(OP_GET_PKTSTATUS, ps, 3);
    *rssi_dbm = -(int)ps[0] / 2;
    *snr_db   = (float)((int8_t)ps[1]) / 4.0f;

    clear_irq(0xFFFF);
    return 1;
}
