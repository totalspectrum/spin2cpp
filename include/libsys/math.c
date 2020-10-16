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

float __builtin_tanf(float x)
{
    return __builtin_sinf(x) / __builtin_cosf(x);
}

// Approximates atan(x) normalized to the [-1,1] range
// with a maximum error of 0.1620 degrees.

static float normalized_atanf( float x )
{
    static const uint32_t sign_mask = 0x80000000;
    static const float b = 0.596227f;

    // Extract the sign bit
    uint32_t ux_s  = sign_mask & *((uint32_t *)&x);

    // Calculate the arctangent in the first quadrant
    float bx_a = __builtin_fabsf( b * x );
    float num = bx_a + x * x;
    float atan_1q = num / ( 1.f + bx_a + num );

    // Restore the sign bit
    uint32_t atan_2q = ux_s | *((uint32_t *)&atan_1q);
    return *((float *)&atan_2q);
}

float __builtin_atanf(float x)
{
    return normalized_atanf(x) * PI_2;
}

// Approximates atan2(y, x) normalized to the -2 to 2 range
// with a maximum error of 0.1620 degrees

static float normalized_atan2f(float y, float x)
{
    static const uint32_t sign_mask = 0x80000000;
    static const float b = 0.596227f;

    // Extract the sign bits
    uint32_t ux_s = sign_mask & *((uint32_t *)&x);
    uint32_t uy_s = sign_mask & *((uint32_t *)&y);

    // Determine the quadrant offset
    int32_t qa = ((ux_s >> 31) * 2);
    int32_t qb = (((ux_s & uy_s) >> 31) * -4);
    float q = (float)(qa + qb);

    // Calculate the arctangent in the first quadrant
    float bxy_a = __builtin_fabsf(b * x * y);
    float num = bxy_a + y * y;
    float atan_1q = num / (x * x + bxy_a + num);

    // Translate it to the proper quadrant
    uint32_t uatan_2q = (ux_s ^ uy_s) | *((uint32_t *)&atan_1q);
    return q + *((float *)&uatan_2q);
}

float __builtin_atan2f(float y, float x)
{
    return normalized_atan2f(y,x) * PI_2;
}

float __builtin_asinf(float x)
{
    float y = __builtin_sqrt(1-x*x);
    return __builtin_atan2f(x, y);
}

float __builtin_acosf(float x)
{
    return PI_2 - __builtin_asinf(x);
}
