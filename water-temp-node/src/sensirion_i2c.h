/*
 * sensirion_i2c.h — the little wire protocol every Sensirion I2C part speaks,
 * shared by sht45.cpp (SHT45) and scd41.cpp (SCD41).
 *
 * Both sensors move data as 16-bit BIG-ENDIAN words, each followed by its own
 * CRC-8 byte (poly 0x31, init 0xFF, no reflection, no final XOR). That checksum
 * is NOT the same one as lora_crc8() in lora/lora_packet.h (poly 0x07/init 0x00)
 * — do not be tempted to share them.
 *
 * Hand-rolled on top of Arduino Wire rather than pulling in the Sensirion
 * libraries, matching how ds18b20.cpp and sx1278.cpp are done here: the WL55
 * env carries no lib_deps at all, and the whole protocol is ~40 lines.
 *
 * Portable across STM32duino boards (F103 prototype + WL55 node) — <Arduino.h>
 * pulls in the right per-family HAL, exactly as ds18b20.h does.
 */
#ifndef SENSIRION_I2C_H
#define SENSIRION_I2C_H

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>

/* CRC-8 (poly 0x31, init 0xFF) over len bytes — the Sensirion word checksum. */
uint8_t sensirion_crc8(const uint8_t *data, uint8_t len);

/*
 * Send a bare 16-bit command word (no arguments). Returns 1 on ACK, else 0.
 *
 * A few SCD41 commands (wake_up) are NOT acked by design; callers that expect
 * that ignore the return value.
 */
int sensirion_cmd(TwoWire *w, uint8_t addr, uint16_t cmd);

/*
 * Send a 16-bit command word followed by one 16-bit argument and its CRC.
 * Returns 1 on ACK, else 0.
 */
int sensirion_cmd_arg(TwoWire *w, uint8_t addr, uint16_t cmd, uint16_t arg);

/*
 * Read `count` words (each: 2 data bytes + 1 CRC byte) into words[].
 * Verifies every CRC; returns 1 only if all of them check out, else 0.
 *
 * Does NOT send a command first — the SHT4x answers a previously issued
 * measurement command, and the SCD4x wants a stop-then-restart between the
 * command and the read, so the caller owns that sequencing.
 */
int sensirion_read_words(TwoWire *w, uint8_t addr, uint16_t *words, uint8_t count);

/*
 * Command-then-read in one call: writes `cmd`, waits `delay_ms`, then reads
 * `count` words. This is the SCD4x pattern. Returns 1 on success, else 0.
 */
int sensirion_read_cmd(TwoWire *w, uint8_t addr, uint16_t cmd,
                       uint16_t *words, uint8_t count, uint16_t delay_ms);

#endif /* SENSIRION_I2C_H */
