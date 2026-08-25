/* sx1278.cpp — see sx1278.h. SX127x LoRa over Arduino SPI. */
#include "sx1278.h"
#include <Arduino.h>
#include <SPI.h>

/* ---- SX127x registers ----------------------------------------------------- */
#define REG_FIFO              0x00
#define REG_OP_MODE           0x01
#define REG_FRF_MSB           0x06
#define REG_FRF_MID           0x07
#define REG_FRF_LSB           0x08
#define REG_PA_CONFIG         0x09
#define REG_LNA               0x0C
#define REG_FIFO_ADDR_PTR     0x0D
#define REG_FIFO_TX_BASE      0x0E
#define REG_FIFO_RX_BASE      0x0F
#define REG_FIFO_RX_CURRENT   0x10
#define REG_IRQ_FLAGS         0x12
#define REG_RX_NB_BYTES       0x13
#define REG_PKT_SNR_VALUE     0x19
#define REG_PKT_RSSI_VALUE    0x1A
#define REG_MODEM_CONFIG1     0x1D
#define REG_MODEM_CONFIG2     0x1E
#define REG_PREAMBLE_MSB      0x20
#define REG_PREAMBLE_LSB      0x21
#define REG_PAYLOAD_LENGTH    0x22
#define REG_MODEM_CONFIG3     0x26
#define REG_SYNC_WORD         0x39
#define REG_DIO_MAPPING1      0x40
#define REG_VERSION           0x42
#define REG_PA_DAC            0x4D

/* OpMode: bit7 = LongRangeMode (LoRa). Low 3 bits = mode. */
#define MODE_LORA             0x80
#define MODE_SLEEP            0x00
#define MODE_STDBY            0x01
#define MODE_TX               0x03
#define MODE_RX_CONTINUOUS    0x05

/* IRQ flags */
#define IRQ_RX_DONE           0x40
#define IRQ_PAYLOAD_CRC_ERR   0x20
#define IRQ_TX_DONE           0x08

static uint8_t s_nss;
static bool    s_low_band;   /* affects the RSSI offset (433 vs 868/915) */

static SPISettings s_spi(2000000, MSBFIRST, SPI_MODE0);

static uint8_t reg_read(uint8_t addr)
{
    SPI.beginTransaction(s_spi);
    digitalWrite(s_nss, LOW);
    SPI.transfer(addr & 0x7F);
    uint8_t v = SPI.transfer(0x00);
    digitalWrite(s_nss, HIGH);
    SPI.endTransaction();
    return v;
}

static void reg_write(uint8_t addr, uint8_t val)
{
    SPI.beginTransaction(s_spi);
    digitalWrite(s_nss, LOW);
    SPI.transfer(addr | 0x80);
    SPI.transfer(val);
    digitalWrite(s_nss, HIGH);
    SPI.endTransaction();
}

static void fifo_write(const uint8_t *buf, uint8_t len)
{
    SPI.beginTransaction(s_spi);
    digitalWrite(s_nss, LOW);
    SPI.transfer(REG_FIFO | 0x80);
    for (uint8_t i = 0; i < len; i++) SPI.transfer(buf[i]);
    digitalWrite(s_nss, HIGH);
    SPI.endTransaction();
}

static void fifo_read(uint8_t *buf, uint8_t len)
{
    SPI.beginTransaction(s_spi);
    digitalWrite(s_nss, LOW);
    SPI.transfer(REG_FIFO & 0x7F);
    for (uint8_t i = 0; i < len; i++) buf[i] = SPI.transfer(0x00);
    digitalWrite(s_nss, HIGH);
    SPI.endTransaction();
}

uint8_t sx127x_read_version(void) { return reg_read(REG_VERSION); }

bool sx127x_begin(const sx127x_pins_t *pins, uint32_t freq_hz)
{
    s_nss      = pins->nss;
    s_low_band = (freq_hz < 600000000UL);

    pinMode(s_nss, OUTPUT);
    digitalWrite(s_nss, HIGH);
    pinMode(pins->reset, OUTPUT);
    if (pins->dio0 != 0xFF) pinMode(pins->dio0, INPUT);

    /* hardware reset: active-low pulse */
    digitalWrite(pins->reset, LOW);
    delay(2);
    digitalWrite(pins->reset, HIGH);
    delay(6);

    SPI.begin();

    /* Enter LoRa mode (LongRangeMode can only change in sleep). */
    reg_write(REG_OP_MODE, MODE_SLEEP);              /* FSK sleep */
    delay(2);
    reg_write(REG_OP_MODE, MODE_LORA | MODE_SLEEP);  /* LoRa sleep */
    delay(2);

    /* frequency: Frf = freq * 2^19 / 32e6 */
    uint32_t frf = (uint32_t)(((uint64_t)freq_hz << 19) / 32000000ULL);
    reg_write(REG_FRF_MSB, (uint8_t)(frf >> 16));
    reg_write(REG_FRF_MID, (uint8_t)(frf >> 8));
    reg_write(REG_FRF_LSB, (uint8_t)(frf));

    reg_write(REG_FIFO_TX_BASE, 0x00);
    reg_write(REG_FIFO_RX_BASE, 0x00);
    reg_write(REG_LNA, 0x23);                        /* max gain + boost */
    reg_write(REG_OP_MODE, MODE_LORA | MODE_STDBY);

    uint8_t ver = reg_read(REG_VERSION);
    return (ver == 0x12);
}

void sx127x_config_lora(uint8_t sf, uint8_t bw_code, uint8_t cr_code,
                        uint16_t preamble, uint8_t syncword, bool crc_on,
                        int8_t power_dbm)
{
    reg_write(REG_OP_MODE, MODE_LORA | MODE_STDBY);

    /* ModemConfig1: BW[7:4] | CR[3:1] | ImplicitHeader(0) */
    reg_write(REG_MODEM_CONFIG1, (uint8_t)((bw_code << 4) | (cr_code << 1)));
    /* ModemConfig2: SF[7:4] | RxCrcOn[2] */
    reg_write(REG_MODEM_CONFIG2, (uint8_t)((sf << 4) | (crc_on ? 0x04 : 0x00)));
    /* ModemConfig3: LowDataRateOptimize[3] (SF>=11 @125k) | AgcAutoOn[2] */
    reg_write(REG_MODEM_CONFIG3, (uint8_t)((sf >= 11 ? 0x08 : 0x00) | 0x04));

    reg_write(REG_PREAMBLE_MSB, (uint8_t)(preamble >> 8));
    reg_write(REG_PREAMBLE_LSB, (uint8_t)(preamble));
    reg_write(REG_SYNC_WORD, syncword);

    if (power_dbm < 2)  power_dbm = 2;
    if (power_dbm > 17) power_dbm = 17;
    reg_write(REG_PA_CONFIG, (uint8_t)(0x80 | (power_dbm - 2)));  /* PA_BOOST pin */
    reg_write(REG_PA_DAC, 0x84);                                  /* normal (<= +17) */
}

bool sx127x_tx(const uint8_t *buf, uint8_t len, uint32_t timeout_ms)
{
    reg_write(REG_OP_MODE, MODE_LORA | MODE_STDBY);
    reg_write(REG_FIFO_ADDR_PTR, 0x00);
    fifo_write(buf, len);
    reg_write(REG_PAYLOAD_LENGTH, len);
    reg_write(REG_DIO_MAPPING1, 0x40);     /* DIO0 = TxDone */
    reg_write(REG_IRQ_FLAGS, 0xFF);        /* clear flags   */
    reg_write(REG_OP_MODE, MODE_LORA | MODE_TX);

    uint32_t t0 = millis();
    while (!(reg_read(REG_IRQ_FLAGS) & IRQ_TX_DONE)) {
        if (millis() - t0 > timeout_ms) return false;
    }
    reg_write(REG_IRQ_FLAGS, IRQ_TX_DONE);
    return true;
}

void sx127x_rx_start(void)
{
    reg_write(REG_DIO_MAPPING1, 0x00);     /* DIO0 = RxDone */
    reg_write(REG_FIFO_ADDR_PTR, 0x00);
    reg_write(REG_IRQ_FLAGS, 0xFF);
    reg_write(REG_OP_MODE, MODE_LORA | MODE_RX_CONTINUOUS);
}

int sx127x_rx_poll(uint8_t *buf, uint8_t max_len, uint8_t *out_len,
                   int *rssi_dbm, float *snr_db)
{
    uint8_t irq = reg_read(REG_IRQ_FLAGS);
    if (!(irq & IRQ_RX_DONE)) return 0;
    reg_write(REG_IRQ_FLAGS, 0xFF);
    if (irq & IRQ_PAYLOAD_CRC_ERR) return -1;

    uint8_t len = reg_read(REG_RX_NB_BYTES);
    if (len > max_len) len = max_len;
    reg_write(REG_FIFO_ADDR_PTR, reg_read(REG_FIFO_RX_CURRENT));
    fifo_read(buf, len);
    *out_len = len;

    /* RSSI offset: -164 dBm low band (433), -157 dBm high band (868/915). */
    *rssi_dbm = (s_low_band ? -164 : -157) + (int)reg_read(REG_PKT_RSSI_VALUE);
    *snr_db   = (float)((int8_t)reg_read(REG_PKT_SNR_VALUE)) / 4.0f;
    return 1;
}
