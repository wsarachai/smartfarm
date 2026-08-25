/* ds18b20.c — see ds18b20.h. Classic 1-Wire bit-bang, timings per the datasheet. */
#include "ds18b20.h"
#include "board.h"

#define CMD_SKIP_ROM        0xCC
#define CMD_CONVERT_T       0x44
#define CMD_READ_SCRATCH    0xBE

static inline void ow_low(const ds_bus_t *b)     { HAL_GPIO_WritePin(b->port, b->pin, GPIO_PIN_RESET); }
static inline void ow_release(const ds_bus_t *b) { HAL_GPIO_WritePin(b->port, b->pin, GPIO_PIN_SET); }
static inline int  ow_read(const ds_bus_t *b)    { return HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_SET; }

void ds18b20_init(const ds_bus_t *bus)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = bus->pin;
    g.Mode  = GPIO_MODE_OUTPUT_OD;   /* open-drain: '1' releases, '0' pulls low */
    g.Pull  = GPIO_NOPULL;           /* external 4.7k on the sensor rail        */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->port, &g);
    ow_release(bus);
}

/* Returns 1 if a presence pulse was detected. */
static int ow_reset(const ds_bus_t *b)
{
    ow_low(b);
    delay_us(480);
    ow_release(b);
    delay_us(70);
    int present = (ow_read(b) == 0);   /* device pulls low = present */
    delay_us(410);
    return present;
}

static void ow_write_bit(const ds_bus_t *b, int bit)
{
    if (bit) {
        ow_low(b);      delay_us(6);
        ow_release(b);  delay_us(64);
    } else {
        ow_low(b);      delay_us(60);
        ow_release(b);  delay_us(10);
    }
}

static int ow_read_bit(const ds_bus_t *b)
{
    int bit;
    ow_low(b);      delay_us(6);
    ow_release(b);  delay_us(9);
    bit = ow_read(b);
    delay_us(55);
    return bit;
}

static void ow_write_byte(const ds_bus_t *b, uint8_t v)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(b, v & 0x01);
        v >>= 1;
    }
}

static uint8_t ow_read_byte(const ds_bus_t *b)
{
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        v >>= 1;
        if (ow_read_bit(b)) v |= 0x80;
    }
    return v;
}

/* Maxim/Dallas 1-Wire CRC-8 (poly x^8+x^5+x^4+1, reflected). */
static uint8_t ds_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t in = data[i];
        for (int j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ in) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            in >>= 1;
        }
    }
    return crc;
}

int ds18b20_start_convert(const ds_bus_t *bus)
{
    if (!ow_reset(bus)) return 0;
    ow_write_byte(bus, CMD_SKIP_ROM);
    ow_write_byte(bus, CMD_CONVERT_T);
    return 1;
}

int ds18b20_read(const ds_bus_t *bus, int16_t *out_c100)
{
    if (!ow_reset(bus)) return 0;
    ow_write_byte(bus, CMD_SKIP_ROM);
    ow_write_byte(bus, CMD_READ_SCRATCH);

    uint8_t sp[9];
    for (int i = 0; i < 9; i++) sp[i] = ow_read_byte(bus);

    if (ds_crc8(sp, 8) != sp[8]) return 0;         /* corrupt / absent probe */

    int16_t raw = (int16_t)(((uint16_t)sp[1] << 8) | sp[0]);
    /* raw is 1/16 C. centi-degC = raw * 100 / 16 = raw * 25 / 4. */
    *out_c100 = (int16_t)(((int32_t)raw * 100) / 16);
    return 1;
}
