/*
 * lora_params.h — LoRa PHY parameters, SHARED between water-temp-node and
 * lora-gateway. These MUST be byte-for-byte identical on both ends or the two
 * radios will never hear each other. If you change one, change the copy in the
 * other project too (see lora-gateway/src/lora/lora_params.h).
 *
 * Region: AS923 (Thailand). Raw point-to-point LoRa — NOT LoRaWAN. There is no
 * duty-cycle / LBT enforcement here; keep the send interval sane (the node wakes
 * every WAKE_INTERVAL_S, default 15 min) so you stay well within AS923 limits.
 */
#ifndef LORA_PARAMS_H
#define LORA_PARAMS_H

/* Carrier frequency in Hz. 923.2 MHz sits in the AS923 sub-band legal in TH. */
#define LORA_FREQ_HZ          923200000UL

/* Spreading factor 5..12. SF9 balances range vs airtime/battery for ~12 bytes. */
#define LORA_SF               9

/* Bandwidth code for SetModulationParams: 0x04=125kHz, 0x05=250, 0x06=500. */
#define LORA_BW_CODE          0x04   /* 125 kHz */

/* Coding rate code: 0x01=4/5, 0x02=4/6, 0x03=4/7, 0x04=4/8. */
#define LORA_CR_CODE          0x01   /* 4/5 */

/* Low-data-rate optimize: required for SF11/SF12 @125k; 0 for SF9. */
#define LORA_LDRO             0x00

/* Preamble length in symbols. 8 is the LoRa default. Both ends must match. */
#define LORA_PREAMBLE_SYMS    8

/*
 * SX126x LoRa sync word (2 bytes, register 0x0740/0x0741).
 * 0x3444 is the value whose SX127x single-byte equivalent is 0x34 (the "0x34"
 * chosen for this project). 0x1424 would be the "private/0x12" equivalent.
 * What matters is that node + gateway use the SAME value.
 */
#define LORA_SYNCWORD_MSB     0x34
#define LORA_SYNCWORD_LSB     0x44

/* TX output power in dBm (node only). AS923 ERP budget — 14 dBm via the LP PA. */
#define LORA_TX_POWER_DBM     14

#endif /* LORA_PARAMS_H */
