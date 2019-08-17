#ifndef _MATH_PRIVATE_H
#define _MATH_PRIVATE_H

#include <compiler.h>
#include <stdint.h>

typedef union __f_or_i {
    float f;
    uint32_t i;
} _FI;

static _INLINE float __asfloat(uint32_t i)
{
    _FI x;
    x.i = i;
    return x.f;
}

static _INLINE uint32_t __asuint(float f) {
    _FI x;
    x.f = f;
    return x.i;
}

#define GET_FLOAT_WORD(i, f) i = __asuint(f)
#define SET_FLOAT_WORD(f, i) f = __asfloat(i)

#endif
