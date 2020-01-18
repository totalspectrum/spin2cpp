#include <stdint.h>

#define PI 3.14159265f
#define PI_2 1.570796327f
#define FIXPT_ONE 1073741824.0f
#define PI_SCALE (FIXPT_ONE / (2.0f*PI))

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

/*
'
' calculate sin(x) where x is an angle in
' 2.30 fixed point, where a whole circle (2pi) is
' 1.0
'
' the ROM lookup table covers the range
' in the range 0 to 2048 (=0x800) (= pi/2)
'
' returns a value from -1.0 to +1.0, again in 2.30 fixed point
'
*/
#ifdef __P2__
int32_t _isin(int32_t x)
{
    int32_t cx, rx, ry;

    x = x<<2;  // convert to 0.32 fixed point
    cx = (1<<30);
    __asm {
        qrotate cx, x
        getqy ry
    }
    return ry;
}

#else
int32_t _isin(int32_t x)
{
    uint16_t *sinptr;
    uint32_t xfrac, sval, nextsval;
    uint32_t q;

    sinptr = (uint16_t *)0xe000; // pointer to sin table in ROM

    // first get x into the correct quadrant
    q = (x >> 28) & 3; // quadrant 0, 1, 2, 3
    if (q & 1) {
        x = -x;
    }

    x = (x>>12) & 0x1ffff; // reduce to 0 - 0xffff
    xfrac = x & (~ 0x1ffe0);
    x = x >> 5;
    sval = sinptr[x];
    // linear interpolation
    nextsval = sinptr[x+1];
    nextsval = ((nextsval - sval) * xfrac) >> 5;
    sval = sval + nextsval;

    if (q & 2) {
        sval = -sval;
    }
    // here sval is -0xffff to +0xffff
    // need to convert
    sval = (sval << 14) / 0xffff;
    return sval << 16;
}

#endif

float __builtin_sinf(float x)
{
    float s;
    x = x * PI_SCALE;
    s = _isin(x) / FIXPT_ONE;
    return s;
}

float __builtin_cosf(float x)
{
    return __builtin_sinf(x + PI_2);
}

float __builtin_tanf(x)
{
    return __builtin_sinf(x) / __builtin_cosf(x);
}
