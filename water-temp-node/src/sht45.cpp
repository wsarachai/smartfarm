/* sht45.cpp — see sht45.h. */
#include "sht45.h"
#include "sensirion_i2c.h"

/* Commands (SHT4x datasheet). Single byte on the wire, but the helper sends a
 * 16-bit word, so they are written here as the byte in the low half — the
 * SHT4x accepts the command as one byte, so we use a dedicated writer below. */
#define CMD_MEASURE_HIGH    0xFD   /* high repeatability, ~8.3 ms typ / 10 ms max */
#define CMD_READ_SERIAL     0x89
#define CMD_SOFT_RESET      0x94   /* ~1 ms */

static TwoWire *s_wire = 0;
static uint8_t  s_addr = SHT45_I2C_ADDR;

/* The SHT4x takes ONE command byte, unlike the SCD4x's 16-bit command words. */
static int sht_cmd(uint8_t cmd)
{
    s_wire->beginTransmission(s_addr);
    s_wire->write(cmd);
    return s_wire->endTransmission() == 0;
}

int sht45_init(TwoWire *w, uint8_t addr)
{
    s_wire = w;
    s_addr = addr;

    if (!sht_cmd(CMD_SOFT_RESET)) return 0;
    delay(2);                       /* datasheet: 1 ms soft-reset time */

    /* Throwaway read: proves the part answers AND that the CRCs check out, so a
     * half-wired bus (SDA floating high, everything "acks") is caught here
     * rather than showing up as a plausible-looking temperature later. */
    int16_t  t;
    uint16_t rh;
    return sht45_read(&t, &rh);
}

int sht45_read(int16_t *out_t_c100, uint16_t *out_rh_x100)
{
    if (!s_wire) return 0;
    if (!sht_cmd(CMD_MEASURE_HIGH)) return 0;
    delay(SHT45_MEASURE_MS);

    uint16_t words[2];
    if (!sensirion_read_words(s_wire, s_addr, words, 2)) return 0;

    /* T[degC]  = -45 + 175 * raw / 65535
     * RH[%]    =  -6 + 125 * raw / 65535
     * Done in int32 centi-units — no FPU on the F103, and the WL55 wakes often
     * enough that pulling in soft-float for two multiplies is not worth it. */
    int32_t t_c100  = (int32_t)(((int64_t)17500 * words[0]) / 65535) - 4500;
    int32_t rh_x100 = (int32_t)(((int64_t)12500 * words[1]) / 65535) - 600;

    if (rh_x100 < 0)     rh_x100 = 0;
    if (rh_x100 > 10000) rh_x100 = 10000;

    if (out_t_c100)  *out_t_c100  = (int16_t)t_c100;
    if (out_rh_x100) *out_rh_x100 = (uint16_t)rh_x100;
    return 1;
}

int sht45_serial(uint32_t *out_serial)
{
    if (!s_wire) return 0;
    if (!sht_cmd(CMD_READ_SERIAL)) return 0;
    delay(2);

    uint16_t words[2];
    if (!sensirion_read_words(s_wire, s_addr, words, 2)) return 0;
    if (out_serial) *out_serial = ((uint32_t)words[0] << 16) | words[1];
    return 1;
}
