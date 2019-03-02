#include <stdint.h>

uint8_t *bumppc(uint8_t *pc, int16_t delta)
{
    return pc + delta;
}

int bump1(int pc, int16_t delta)
{
    return pc + delta;
}

unsigned bump2(unsigned pc, int16_t delta)
{
    return pc + delta;
}
