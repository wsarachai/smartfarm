/* scd41.cpp — see scd41.h. */
#include "scd41.h"
#include "sensirion_i2c.h"

/* Command words (SCD4x datasheet, section "Command Overview"). */
#define CMD_START_PERIODIC      0x21B1
#define CMD_READ_MEASUREMENT    0xEC05   /* 1 ms exec */
#define CMD_STOP_PERIODIC       0x3F86   /* 500 ms exec */
#define CMD_SET_AMBIENT_PRESS   0xE000
#define CMD_GET_DATA_READY      0xE4B8   /* 1 ms exec */
#define CMD_GET_SERIAL          0x3682   /* 1 ms exec */
#define CMD_MEASURE_SINGLE_SHOT 0x219D   /* 5000 ms exec, SCD41 only */

static TwoWire *s_wire = 0;
static uint8_t  s_addr = SCD41_I2C_ADDR;

int scd41_init(TwoWire *w, uint8_t addr)
{
    s_wire = w;
    s_addr = addr;

    /* The sensor keeps measuring across an MCU reset, and while it is measuring
     * it NAKs nearly everything. Always stop first, and ignore the result — on
     * a sensor that was already idle this legitimately does nothing. */
    sensirion_cmd(s_wire, s_addr, CMD_STOP_PERIODIC);
    delay(SCD41_STOP_PERIODIC_MS);

    uint64_t serial;
    return scd41_serial(&serial);
}

int scd41_start_periodic(void)
{
    if (!s_wire) return 0;
    return sensirion_cmd(s_wire, s_addr, CMD_START_PERIODIC);
}

int scd41_stop_periodic(void)
{
    if (!s_wire) return 0;
    int ok = sensirion_cmd(s_wire, s_addr, CMD_STOP_PERIODIC);
    delay(SCD41_STOP_PERIODIC_MS);
    return ok;
}

int scd41_data_ready(int *ready)
{
    if (!s_wire) return 0;

    uint16_t w;
    if (!sensirion_read_cmd(s_wire, s_addr, CMD_GET_DATA_READY, &w, 1, 2)) return 0;

    /* Datasheet: data is ready unless the low 11 bits are all zero. The top 5
     * bits are undefined and must be masked off, not compared. */
    if (ready) *ready = (w & 0x07FF) != 0;
    return 1;
}

/* Convert the raw 3-word measurement block into engineering units. */
static void scd41_convert(const uint16_t *words, uint16_t *co2,
                          int16_t *t_c100, uint16_t *rh_x100)
{
    if (co2) *co2 = words[0];                       /* already ppm */

    /* T[degC] = -45 + 175 * raw / 65535  (same transfer function as the SHT4x)
     * RH[%]   =       100 * raw / 65535  (note: no -6 offset, unlike the SHT4x) */
    if (t_c100) {
        int32_t t = (int32_t)(((int64_t)17500 * words[1]) / 65535) - 4500;
        *t_c100 = (int16_t)t;
    }
    if (rh_x100) {
        int32_t rh = (int32_t)(((int64_t)10000 * words[2]) / 65535);
        if (rh > 10000) rh = 10000;
        *rh_x100 = (uint16_t)rh;
    }
}

int scd41_read(uint16_t *out_co2_ppm, int16_t *out_t_c100, uint16_t *out_rh_x100)
{
    if (!s_wire) return 0;

    uint16_t words[3];
    if (!sensirion_read_cmd(s_wire, s_addr, CMD_READ_MEASUREMENT, words, 3, 2)) return 0;

    scd41_convert(words, out_co2_ppm, out_t_c100, out_rh_x100);
    return 1;
}

int scd41_read_single_shot(int warmup, uint16_t *out_co2_ppm,
                           int16_t *out_t_c100, uint16_t *out_rh_x100)
{
    if (!s_wire) return 0;

    if (warmup) {
        /* Throwaway: the first shot after power-up reads low/unsettled. We do
         * not even read the result back — the point is only to run one cycle
         * through the photoacoustic cell. */
        if (!sensirion_cmd(s_wire, s_addr, CMD_MEASURE_SINGLE_SHOT)) return 0;
        delay(SCD41_SINGLE_SHOT_MS);
    }

    if (!sensirion_cmd(s_wire, s_addr, CMD_MEASURE_SINGLE_SHOT)) return 0;
    delay(SCD41_SINGLE_SHOT_MS);

    return scd41_read(out_co2_ppm, out_t_c100, out_rh_x100);
}

int scd41_set_ambient_pressure(uint16_t hpa)
{
    if (!s_wire) return 0;
    /* The command takes pressure in Pa/100, i.e. hPa directly. */
    return sensirion_cmd_arg(s_wire, s_addr, CMD_SET_AMBIENT_PRESS, hpa);
}

int scd41_serial(uint64_t *out_serial)
{
    if (!s_wire) return 0;

    uint16_t words[3];
    if (!sensirion_read_cmd(s_wire, s_addr, CMD_GET_SERIAL, words, 3, 2)) return 0;

    if (out_serial) {
        *out_serial = ((uint64_t)words[0] << 32) |
                      ((uint64_t)words[1] << 16) |
                       (uint64_t)words[2];
    }
    return 1;
}
