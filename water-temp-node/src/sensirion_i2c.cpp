/* sensirion_i2c.cpp — see sensirion_i2c.h. */
#include "sensirion_i2c.h"

uint8_t sensirion_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

int sensirion_cmd(TwoWire *w, uint8_t addr, uint16_t cmd)
{
    w->beginTransmission(addr);
    w->write((uint8_t)(cmd >> 8));
    w->write((uint8_t)(cmd & 0xFF));
    return w->endTransmission() == 0;
}

int sensirion_cmd_arg(TwoWire *w, uint8_t addr, uint16_t cmd, uint16_t arg)
{
    uint8_t a[2] = { (uint8_t)(arg >> 8), (uint8_t)(arg & 0xFF) };

    w->beginTransmission(addr);
    w->write((uint8_t)(cmd >> 8));
    w->write((uint8_t)(cmd & 0xFF));
    w->write(a, 2);
    w->write(sensirion_crc8(a, 2));
    return w->endTransmission() == 0;
}

int sensirion_read_words(TwoWire *w, uint8_t addr, uint16_t *words, uint8_t count)
{
    const uint8_t nbytes = (uint8_t)(count * 3);

    if (w->requestFrom((int)addr, (int)nbytes) != nbytes) return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t b[2];
        b[0] = (uint8_t)w->read();
        b[1] = (uint8_t)w->read();
        uint8_t crc = (uint8_t)w->read();
        if (sensirion_crc8(b, 2) != crc) return 0;
        words[i] = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
    }
    return 1;
}

int sensirion_read_cmd(TwoWire *w, uint8_t addr, uint16_t cmd,
                       uint16_t *words, uint8_t count, uint16_t delay_ms)
{
    if (!sensirion_cmd(w, addr, cmd)) return 0;
    if (delay_ms) delay(delay_ms);
    return sensirion_read_words(w, addr, words, count);
}
