#include <stdint.h>

typedef union f_or_i {
    float f;
    uint32_t i;
} FI;

float __builtin_fabsf(float f)
{
    FI x;
    x.f = f;
    x.i &= 0x7fffffff;
    return x.f;
}
