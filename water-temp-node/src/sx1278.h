/*
 * sx1278.h — minimal SX127x (SX1276/77/78/79) LoRa driver over Arduino SPI.
 *
 * For an EXTERNAL SX127x module (e.g. Ra-02 / SX1278 @ 433 MHz) on an MCU with a
 * SPI bus — used here for a bench self-test on the STM32F103C8T6 while the
 * NUCLEO-WL55JC1 boards are on back-order. This is a DIFFERENT chip/register map
 * from the WL55's internal SX126x (src/lora/subghz_lora.*), so it is a separate
 * driver; it is NOT used by the shipped WL55 node.
 *
 * NOTE: SX1278 is a 137-525 MHz (low-band) part — it cannot reach the project's
 * 923 MHz AS923 gateway. This driver takes freq_hz so it also works with a
 * high-band SX1276 (868/915/923) module if you have one.
 */
#ifndef SX1278_H
#define SX1278_H

#include <stdint.h>

typedef struct {
    uint8_t nss;    /* chip select (GPIO)      */
    uint8_t reset;  /* reset, active-low (GPIO)*/
    uint8_t dio0;   /* TxDone/RxDone (GPIO)    */
} sx127x_pins_t;

/* Reset the module, start SPI, put it in LoRa standby at freq_hz.
 * Returns true if the chip answers with the expected RegVersion (0x12). */
bool sx127x_begin(const sx127x_pins_t *pins, uint32_t freq_hz);

/* Raw RegVersion (reg 0x42). SX1276/77/78/79 return 0x12; 0x00/0xFF => wiring. */
uint8_t sx127x_read_version(void);

/* Configure the LoRa modem. bw_code: 7=125k 8=250k 9=500k. cr_code: 1=4/5..4=4/8.
 * sf 6-12. syncword 1 byte (0x12 private / 0x34 "public"). power dBm 2..17 (PA_BOOST). */
void sx127x_config_lora(uint8_t sf, uint8_t bw_code, uint8_t cr_code,
                        uint16_t preamble, uint8_t syncword, bool crc_on,
                        int8_t power_dbm);

/* Blocking transmit of up to 255 bytes; polls TxDone. Returns true on TxDone. */
bool sx127x_tx(const uint8_t *buf, uint8_t len, uint32_t timeout_ms);

/* Put the modem in continuous RX. Call once before polling sx127x_rx_poll(). */
void sx127x_rx_start(void);

/* Non-blocking RX poll. On a CRC-valid frame, copies up to max_len bytes, sets
 * *out_len, *rssi_dbm, *snr_db, returns 1. 0 = nothing yet, -1 = CRC error. */
int sx127x_rx_poll(uint8_t *buf, uint8_t max_len, uint8_t *out_len,
                   int *rssi_dbm, float *snr_db);

#endif /* SX1278_H */
