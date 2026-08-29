/* tca9548a.cpp — see tca9548a.h. The whole device is one write-only byte. */
#include "tca9548a.h"

static int write_mask(TwoWire *w, uint8_t addr, uint8_t mask)
{
    if (!w) return 0;
    w->beginTransmission(addr);
    w->write(mask);
    return w->endTransmission() == 0;
}

int tca9548a_select(TwoWire *w, uint8_t addr, uint8_t channel)
{
    if (channel > 7) return 0;
    /* Exactly one bit: the three SHT45s must never be bridged together, which
     * is the entire reason this part is on the board. */
    return write_mask(w, addr, (uint8_t)(1u << channel));
}

int tca9548a_none(TwoWire *w, uint8_t addr)
{
    return write_mask(w, addr, 0x00);
}
