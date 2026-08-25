/*
 * subghz_lora.h — minimal raw-LoRa driver for the STM32WL55 internal SubGHz
 * radio, SHARED between water-temp-node and lora-gateway. Keep identical in both
 * projects (copy in lora-gateway/src/lora/).
 *
 * This is a lean, self-contained SX126x command driver that talks to the radio
 * through ST's HAL_SUBGHZ API (SUBGHZSPI). It deliberately does NOT vendor the
 * full STM32CubeWL SubGHz_Phy middleware — we only need fixed-config one-shot TX
 * (node) and continuous RX (gateway) on a single hard-coded channel.
 *
 * Board assumptions are for the NUCLEO-WL55JC1 (MB1389), taken from ST's BSP:
 *   RF switch  FE_CTRL1=PC4, FE_CTRL2=PC5, FE_CTRL3=PC3 (OUTPUT_PP, no pull)
 *   TCXO       powered by the radio via DIO3 (SetDIO3AsTcxoCtrl), 1.7 V
 *   Regulator  DC-DC (SMPS) supported
 * If you port to another WL board, fix subghz_lora_rf_switch()/TCXO here.
 */
#ifndef SUBGHZ_LORA_H
#define SUBGHZ_LORA_H

#include <stdint.h>
#include <stddef.h>

/* Call once after HAL init + system clock. Resets/configures the radio for the
 * shared LoRa channel (see lora_params.h). Returns 0 on success, <0 on error. */
int subghz_lora_init(void);

/* Blocking single-shot transmit of up to 255 bytes. Sets the RF switch to the
 * TX (RFO_LP) path, sends, waits for TxDone/Timeout, then parks the switch OFF.
 * Returns 0 on TxDone, <0 on timeout/error. Node side. */
int subghz_lora_send(const uint8_t *buf, uint8_t len, uint32_t timeout_ms);

/* Cold-sleep the radio (lowest current) before the node enters Stop2. The next
 * subghz_lora_init() fully reconfigures it. Node side. */
void subghz_lora_sleep(void);

/* Put the radio into continuous RX (RF switch -> RX path). Gateway side. */
int subghz_lora_recv_start(void);

/*
 * Non-blocking receive poll. If a CRC-valid frame arrived, copies up to max_len
 * bytes into buf, sets *out_len, *rssi_dbm (int, dBm) and *snr_db (float, dB),
 * and returns 1. Returns 0 if nothing yet, <0 on a hardware error. The radio
 * stays in continuous RX afterwards (no re-arm needed). Gateway side.
 */
int subghz_lora_recv_poll(uint8_t *buf, uint8_t max_len, uint8_t *out_len,
                          int *rssi_dbm, float *snr_db);

#endif /* SUBGHZ_LORA_H */
