/* 
 * efficient 32 bit floating point in C
 * Copyright 2021 Total Spectrum Software Inc.
 */

#include <stdint.h>

#define FLOAT_ONE (1<<23)
#define FLOAT_TWO (2<<23)
#define DEFAULT_NAN asFloat(0x7ff00000)
#define FLAG_NEG  0x01
#define FLAG_INF  0x02
#define FLAG_NAN  0x04
#define FLAG_ZERO 0x08

// utility functions
union fori { float f; uint32_t i; };

static float asFloat(uint32_t i) { union fori fi; fi.i = i; return fi.f; }
static uint32_t asInt(float f) { union fori fi; fi.f = f; return fi.i; }

// function to count leading zeros
#define CLZ(x) __builtin_clz(x)

//
// unpack a float f into "exp", "s", and "flags"
// where "exp" is the exponent
//       "s" is the significand
//       "flags" has the sign and potentially other flags
//       all of s, and flags must be uint32_t and distinct
//              exp must be int
//       NOTE: handling the implicit 1 bit must be done by other code
//             which must also handle INF, NAN, and other special cases
//
#define UNPACK(f, s, exp, flags)         \
    {                                    \
        flags = asInt(f);                \
        s = (flags << 9) >> 9;           \
        exp = (flags << 1) >> 24;        \
        flags >>= 31;                    \
    }

//
// pack exp, a, alo, and flags into a final floating point number
//  (a, alo) is the 9.23+32 representation of the number
//
static float
pack(uint32_t a, uint32_t alo, int exp, int flags)
{
    exp = exp + 127;

    if (flags & FLAG_NAN) {
        if (a == 0) a = 0x400000;
        a = a | 0x7f800000;
    } else if ( flags & FLAG_INF) {
        a = 0x7f800000;
        alo = 0;
    } else if (flags & FLAG_ZERO) {
        a = 0;
    } else if (exp >= 0xff) {
        // overflow
        a = 0x7f800000;
        alo = 0;
    } else if (exp <= 0) {
        // need a denormalized number
        uint32_t sticky;
        alo = (alo >> 1) | ((a & 1) << 31);
        a = a>>1;
        while (exp < 0 && a != 0) {
            sticky = alo & 1;
            exp++;
            alo = (alo>>1) | ((a&1)<<31) | sticky;
            a = a>>1;
        }

        if (exp < 0) {
            // a got to 0, so we're going to underflow
            alo = alo != 0;
        }
    } else {
        a = (a & ~0xff800000);
        a |= (exp<<23);
    }

    // round off
    {
        uint32_t oldlo;
        if ( (a & 1) ) {
            alo |= 1;
        }
        oldlo = alo;
        alo += 0x7fffffff;
        a += (alo < oldlo);
    }

    // and set sign
    if (flags & FLAG_NEG) {
        a |= 0x80000000; // set sign bit
    }
    return asFloat(a);
}

// calculate 32*32 -> 64 bit multiplication:
// a * b -> (desthi, destlo)
#ifdef __FLEXC__

#define MULTIPLY_LONG(desthi, destlo, a, b) \
    {                                       \
        destlo = a*b;                       \
        desthi = __builtin_muluh(a, b);     \
    }

#else

#define MULTIPLY_LONG(desthi, destlo, a, b) \
    {                                       \
        uint64_t t = a * (uint64_t)b;       \
        desthi = (uint32_t)((t >> 32));     \
        destlo = (uint32_t)t;               \
    }

#endif

// calculate 64 bit division ((nhi<<32)+nlo)/d as a fraction q
typedef struct DoubleUint_ {
    uint32_t lo;
    uint32_t hi;
} DoubleUint;

#ifdef __FLEXC__
#define DIVIDE_LONG(q, r, nhi, nlo, d)              \
    {                                               \
        DoubleUint s = _div64(nhi, nlo, d);         \
        q = s.lo;                                   \
        r = s.hi;                                   \
    }
#else
#define DIVIDE_LONG(q, r, nhi, nlo, d)        \
    {                                     \
        uint64_t t = (((uint64_t)nhi)<<32)|nlo;     \
        q = t / d;                        \
        r = t % d;                        \
    }
#endif

// calculate 64 bit addition
// (desthi, destlo) += (srchi, srclo)
#ifdef __FLEXC__
#define ADD_LONG(desthi, destlo, srchi, srclo)     \
    __asm {                                        \
        add destlo, srclo wc;                      \
        addx desthi, srchi;                        \
    }
#else
#define ADD_LONG(desthi, destlo, srchi, srclo)      \
    {                                               \
        uint32_t oldlo = destlo;                    \
        destlo += srclo;                            \
        desthi += srchi;                            \
        desthi += (srclo < oldlo);                  \
    }
#endif

// negate the pair xhi, xlo
#define NEG_LONG(xhi, xlo)     \
    {                          \
        xhi = ~xhi;            \
        xlo = ~xlo;            \
        xlo += 1;              \
        xhi += (xlo == 0);     \
    }

//
// calculate a*b
//
float _float_mul(float af, float bf)
{
    uint32_t a, b;
    int32_t aexp, bexp;
    uint32_t ahi, alo;
    uint32_t aflag, bflag;
    int exp;

    UNPACK(af, a, aexp, aflag);
    UNPACK(bf, b, bexp, bflag);
    alo = 0;
    
    // calculate sign of a*b
    aflag = aflag ^ bflag;
    // check for various special cases
    if (aexp == 0xff) {
        goto a_overflow;
    }
    if (bexp == 0xff) {
        goto b_overflow;
    }
    if (aexp == 0) {
        goto a_underflow;
    }
    a |= FLOAT_ONE;
a_ok:
    if (bexp == 0) {
        goto b_underflow;
    }
    b |= FLOAT_ONE;
calc:    
    exp = aexp + bexp - (127*2);

// input numbers are 1.23 fixed point
// so output t would be 2.46 fixed point
// but we want an output of 2.55 (2.23 + 0.32)
// so shift up a total of 9

    MULTIPLY_LONG(ahi, alo, (a<<4), (b<<5));

    // we know 1.0 <= a.s, b.s < 2, so a.s * b.s could be big enough to need normalizing
    if (ahi >= (2<<23)) {
        exp++;
        alo = (alo >> 1) | (ahi << 31);
        ahi = ahi >> 1;
    }

    // return
    return pack(ahi, alo, exp, aflag);

    // various special cases
a_overflow:
    // a is NaN or infinite
    if (a != 0) {
        // NaN
        return af;
    }
    if (bexp < 0xff) {
        // b was finite, a infinite -> return a
        if (bexp == 0 && b == 0) {
            // 0 * infinity -> NaN
            return DEFAULT_NAN;
        }
    } else if (b != 0) {
        // b was NaN
        return bf;
    }
    // a*b is infinite: return correctly signed value
    return pack(0, 0, aexp, aflag | FLAG_INF);
b_overflow:
    // we already know that a was finite, so just return b
    // unless a was 0!
    if (aexp == 0 && a == 0) {
        return DEFAULT_NAN;
    }
    if (b != 0) {
        return bf; // b was NaN
    }
    return pack(0, 0, bexp, aflag | FLAG_INF);
a_underflow:
    if (a != 0) {
        // denormalized number
        a = a<<1;
        while (a < FLOAT_ONE) {
            aexp--;
            a = a<<1;
        }
        goto a_ok;
    }   
    // a is 0, b is not infinite, so return correctly signed 0
    return pack(0, 0, 0, aflag | FLAG_ZERO);
b_underflow:
    if (b != 0) {
        // denormalized number
        b = b<<1;
        while (b < FLOAT_ONE) {
            bexp--;
            b = b<<1;
        }
        goto calc;
    }
    return pack(0, 0, 0, aflag | FLAG_ZERO);
}

//
// calculate a / b
//
float _float_div(float af, float bf)
{
    uint32_t a, b;
    int32_t aexp, bexp;
    uint32_t ahi, alo;
    uint32_t aflag, bflag;
    uint32_t q, r, tmpdiv;
    int exp;

    UNPACK(af, a, aexp, aflag);
    UNPACK(bf, b, bexp, bflag);
    alo = 0;
    
    // calculate sign of a*b
    aflag = aflag ^ bflag;
    // check for various special cases
    if (aexp == 0xff) {
        goto a_overflow;
    }
    if (bexp == 0xff) {
        goto b_overflow;
    }
    if (aexp == 0) {
        goto a_underflow;
    }
    a |= FLOAT_ONE;
a_ok:
    if (bexp == 0) {
        goto b_underflow;
    }
    b |= FLOAT_ONE;
calc:
    exp = aexp - bexp;
    ahi = a >> 2;
    alo = a << 30;
    DIVIDE_LONG(q, r, ahi, alo, b);
    // convert from 2.30 to 9.23
    alo = (q << 25) | (r != 0);
    ahi = (q >> 7);
    if (ahi >= (2<<23)) {
        exp++;
        alo = (alo >> 1) | (ahi << 31);
        ahi = ahi >> 1;
    } else if (ahi < (1<<23)) {
        --exp;
        ahi = (ahi << 1) | (alo>>31);
        alo = alo<<1;
    }
    return pack(ahi, alo, exp, aflag);

    /////////////////////////////////////////
    // various special cases
a_overflow:
    // a is NaN or infinite
    if (a != 0) {
        // NaN
        return af;
    }
    if (bexp == 0xff) {
        // we have either inf / inf or inf / nan
        return DEFAULT_NAN;
    }
    // a / b is infinite: return correctly signed value
    return pack(0, 0, aexp, aflag | FLAG_INF);
b_overflow:
    // we already know that a was finite, so just return 0
    if (b != 0) {
        return bf; // b was NaN
    }
    return pack(0, 0, 0, aflag | FLAG_ZERO);
a_underflow:
    if (a != 0) {
        // denormalized number
        a = a<<1;
        while (a < FLOAT_ONE) {
            aexp--;
            a = a<<1;
        }
        goto a_ok;
    }   
    // a is 0, b is not infinite, so return correctly signed 0
    // EXCEPTION: if b is 0, return NaN
    if (bexp == 0 && b == 0) {
        return DEFAULT_NAN;
    }
    return pack(0, 0, 0, aflag | FLAG_ZERO);
b_underflow:
    if (b != 0) {
        // denormalized number
        b = b<<1;
        while (b < FLOAT_ONE) {
            bexp--;
            b = b<<1;
        }
        goto calc;
    }
    // a / 0 -> INFINITY (unless a == 0, but we already handled that case)
    return pack(0, 0, 0, aflag | FLAG_INF);
}

float
_float_add(float af, float bf)
{
    uint32_t a, alo, aexp, aflag;
    uint32_t b, blo, bexp, bflag;
    int delta;
    uint32_t negflag;
    
    UNPACK(af, a, aexp, aflag);
    UNPACK(bf, b, bexp, bflag);
    alo = blo = 0;
    // swap if necessary so |a| > |b|
    if ( (aexp < bexp) || ( (aexp == bexp) && (a < b) ) ) {
        uint32_t tmp;
        tmp = a; a = b; b = tmp;
        tmp = aexp; aexp = bexp; bexp = tmp;
        tmp = aflag; aflag = bflag; bflag = tmp;
    }
    if (aexp == 0xff) {
        goto a_overflow;
    }
    if (aexp == 0) {
        goto a_underflow;
    }
    a |= FLOAT_ONE;
a_ok:
    if (bexp == 0) {
        goto b_underflow;
    }
    b |= FLOAT_ONE;
calc:
    aexp -= 127;
    bexp -= 127;
    delta = aexp - bexp;
    // shift b right as needed
    while (delta >= 32) {
        blo = b | (blo != 0);
        b = 0;
        delta -= 32;
    }
    if (delta) {
        unsigned t1 = b << (32-delta);
        unsigned t2 = blo << (32-delta);
        blo = blo >> delta;
        b = b >> delta;
        blo |= t1;
        blo |= (t2 != 0); // sticky bit
    }
    // now actually do the calculation
    // see if a and b have the same signs
    negflag = (aflag ^ bflag) & FLAG_NEG;
    if (negflag) {
        NEG_LONG(b, blo);
    }
    {
        // add the numbers together
        uint32_t sticky;
        ADD_LONG(a, alo, b, blo);

        // check for negative result
        if (0 > (int32_t)a) {
            aflag ^= FLAG_NEG;
            NEG_LONG(a, alo);
        }
        if (a >= FLOAT_TWO) {
            aexp++;
            sticky = alo & 1;
            alo = (alo >> 1) | (a << 31) | sticky;
            a = a >> 1;
        } else if (a < FLOAT_ONE) {
            if (a == 0 && alo == 0) {
                aflag |= FLAG_ZERO;
                aflag &= ~FLAG_NEG;
            } else {
                while (a < FLOAT_ONE) {
                    ADD_LONG(a, alo, a, alo);
                    --aexp;
                }
            }
        }
    }
    return pack(a, alo, aexp, aflag);

    ///////////////////////////
    // special cases
    ///////////////////////////
a_overflow:
    // infinity: check for inf - inf special case
    if (bexp == 0xff && bflag != aflag) {
        return DEFAULT_NAN;
    }
    if (a != 0) {
        return DEFAULT_NAN;
    } 
    // otherwise infinity +- x -> infinity
    return pack(a, alo, aexp, aflag | FLAG_INF);
a_underflow:
    if (a != 0) {
        // denormalized number
        a = a<<1;
        while (a < FLOAT_ONE) {
            aexp--;
            a = a<<1;
        }
        goto a_ok;
    }   
    // here a is 0, and so b must be 0 too
    aflag &= bflag;
    return pack(0, 0, 0, aflag | FLAG_ZERO);
b_underflow:
    if (b != 0) {
        // denormalized number
        // denormalized number
        b = b<<1;
        while (b < FLOAT_ONE) {
            bexp--;
            b = b<<1;
        }
        goto calc;
    }
    return pack(a, alo, aexp - 127, aflag);
}

float _float_sqrt(float af)
{
    uint32_t a, alo, aflag;
    int aexp;
    UNPACK(af, a, aexp, aflag);

    // check for sqrt(0)
    if (aexp == 0) {
        goto a_underflow;
    }
    a |= FLOAT_ONE;
calc:    
    // check for sqrt(negative number)
    if (aflag) {
        return DEFAULT_NAN;
    }
    // check for sqrt(inf) or sqrt(nan)
    if (aexp == 0xff) {
        return af;
    }
    aexp -= 127;
    if (aexp & 1) {
        a = a<<1;
        --aexp;
    }
    aexp = aexp/2;
    // a is currently 2.23
    // shift it up to 2.28
    a = a<<5;
    a = _sqrt64(a, 0);
    // a is now 1.30
    // take it down to .23
    alo = a<<25;
    a = a>>7;
    //__builtin_printf("... rescaled: aexp=%d a=%08x alo=%08x\n", aexp, a, alo);
    if (a > FLOAT_TWO) {
        ++aexp;
        a=a>>1;
        //__builtin_printf("... adjust: aexp=%d a=%08x alo=%08x\n", aexp, a, alo);
    }
    return pack(a, alo, aexp, aflag);

a_underflow:
    if (a == 0) {
        // sqrt(0) or sqrt(-0) should return original value
        return af;
    }
    // denormalized number
    a = a<<1;
    while (a < FLOAT_ONE) {
        aexp--;
        a=a<<1;
    }
    goto calc;
}

//
// calculate b*a^n with maximum precision, where n is an integer
//
float _float_pow_n(float b, float a, long n)
{
    int invflag;
    float r;
    if (n < 0) {
        invflag = 1;
        n = -n;
        if (n < 0) {
            return 0;
        }
    } else {
        invflag = 0;
    }
    r = 1.0f;
    while (n > 0) {
        if (n & 1) {
            r = r * a;
        }
        n = n>>1;
        a = a*a;
    }
    if (invflag) {
        r = b / r;
    } else if (b != 1.0f) {
        r = b * r;
    }
    return r;
}
