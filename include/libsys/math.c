#include <stdint.h>

#define PI 3.14159265f
#define PI_2 1.570796327f
#ifdef __fixedreal__
#define FIXPT_ONE 16384.0f /* 1<<14 */
#else
#define FIXPT_ONE 1073741824.0f /* 1<<30 */
#endif
#define PI_SCALE (FIXPT_ONE / (2.0f*PI))

typedef union f_or_i {
    float f;
    uint32_t i;
} FI;

static uint32_t __asuint(float f) {
    FI u;
    u.f = f;
    return u.i;
}
static float __asfloat(uint32_t i) {
    FI u;
    u.i = i;
    return u.f;
}

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

    //__builtin_printf(" [x=%08x] ", x);
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
#ifdef __fixedreal__
    //__builtin_printf(" [f=%f] ", x);
    s = __asfloat(_isin(__asuint(x)) >> 14);
#else    
    s = _isin(x) / FIXPT_ONE;
#endif    
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

#ifdef __P2__
// ATAN2 using the P2 CORDIC hardware
#define SCALE ((float)(1<<30))

// returns a value between -1 and +1, where
// -1 corresponds to -PI and +1 to +PI
int32_t _qpolar(int32_t x, int32_t y)
{
    int32_t angle;
    __asm {
        qvector x, y
        getqy angle
        sar   angle, #1  /* convert from 1.31 to 2.30 */
    };
    //__builtin_printf("  ..polar(%x,%x) -> %x (%d)\n", x, y, angle, angle); 
    return angle;
}

float __builtin_atan2f(float y, float x)
{
    float r;
    int32_t a, b, c;
    if (y == 0) {
        if (x < 0) {
            return -PI;
        } else {
            return 0;
        }
    }
    r = __builtin_sqrt(x*x + y*y);
    x = x / r;
    y = y / r;
    /* now -1.0 <= x,y <= 1.0 */
    /* convert to 2.30 fixed point */
    a = SCALE * x;
    b = SCALE * y;
    c = _qpolar(a, b);
    r = PI * (c / SCALE);
    return r;
}

#else

/* fast ATAN2 approximation derived from Jim Shima's fixed point Atan2
 * http://dspguru.com/dsp/tricks/fixed-point-atan2-with-self-normalization/
 */
float __builtin_atan2f(float y, float x)
{
    float coeff_1 = PI/4.0f;
    float coeff_2 = (3.0f*PI)/4.0f;
    float abs_y = __builtin_abs(y);
    float r, angle;
    
    if (x >= 0) {
        r = (x - abs_y) / (x + abs_y);
        angle = coeff_1;
    } else {
        r = (x + abs_y) / (abs_y - x);
        angle = coeff_2;
    }
    angle += (0.1963f * r * r - 0.9817f) * r;
    if (y < 0) {
        return -angle;
    }
    return angle;
}

#endif

// note: atan(y/x) == atan2(y, x)
float __builtin_atanf(float y)
{
    return __builtin_atan2f(y, 1.0f);
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
