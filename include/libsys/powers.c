//
// code for doing exponentials and logarithms
// Copyright 2020 Total Spectrum Software Inc.
// MIT licensed
//

#define CONST_E 2.71828182846f
#define LOG2_10 3.32192809489f
#define LOG2_E  1.4426950409f
#define LOGE_2  0.69314718056f

// utilities
typedef union ForU {
    float f;
    unsigned u;
} ForU;

static inline unsigned __asuint(float f) { ForU u; u.f = f; return u.u; }
static inline float __asfloat(unsigned d) { ForU u; u.u = d; return u.f; }

#ifdef __P2__

// calculate log2(mant) as a 5.27 fixed point number
unsigned __builtin_qlog(unsigned mant)
{
    unsigned r;
    __asm {
        qlog mant
        getqx r
    };
    return r;
}

// calculate 2^mant, where mant is a 5.27  fixed point number
unsigned __builtin_qexp(unsigned mant)
{
    unsigned r;
    __asm {
        qexp mant
        getqx r
    };
    return r;
}
#else
// calculate log base 2 of an unsigned num as a 5.27 fixed point number
// uses the lookup table in ROM
// algorithm comes from Propeller manual
unsigned __builtin_qlog(unsigned num)
{
    unsigned exp;
    unsigned r;
    unsigned r2;
    unsigned frac;
    exp = __builtin_clz(num);
    num = num << exp;  // left justify
    // we will look up based on 11 bits, so the bottom 16 bits are the fraction
    frac = num & 0xffff;
    num = (num >> (30-11)) & 0xffe;  // 30 because we will multiply by 2 for word access
    r = *((unsigned short *)(0xC000 + num));
    r2 = *((unsigned short *)(0xC002 + num));
    r = r + (((r2-r)*frac) >> 16);
    exp = exp ^ 0x1f;
    r = r | (exp << 16);

    r = r << (27-16);
    return r;
}
// calculate 2^num, where "num" is a 5.27 fixed point num
// uses the lookup table in ROM
// algorithm comes from Propeller manual
unsigned __builtin_qexp(unsigned orig_num)
{
    unsigned exp;
    unsigned r, r2;
    unsigned frac;
    unsigned num = orig_num;
    
    exp = (num >> 27);
    // the  original value is 5.27
    // we will look up based on 11 bits, so the bottom 16 bits are the fraction
    frac = num & 0xffff;
    num = (num >> (26-11)) & 0x0ffe;
    //__builtin_printf("...num=0x%08x lookup=%08x exp=%d\n", orig_num, num, exp);
    r = *((unsigned short *)(0xD000 + num));
    r2 = *((unsigned short *)(0xD002 + num));
    //__builtin_printf("...r1=0x%08x r2=%08x frac=%04x\n", r, r2, frac);
    r = r + ((frac * (r2-r)) >> 16);
    r = r << 15; // shift into 30..15
    r |= 0x80000000;
    //printf("...r=0x%08x\n", r);
    exp ^= 0x1f;
    r = r >> exp;
    return r;
}

#endif

//
// floating point: calculate log base 2 of a float number
//
float __builtin_log2f(float x)
{
    unsigned u = __asuint(x);
    int exp;
    unsigned mant;
    unsigned r;
    int rexp;
    float n;
    
    if (u == 0 || u == 0x80000000) {
        return -__builtin_inf();
    }
    if ((int)u < 0) {
        return __builtin_nan("");
    }

    // some special cases
    if (x == 10.0) {
        return LOG2_10;
    }
    if (x == CONST_E) {
        return LOG2_E;
    }

#ifdef __fixedreal__
    // construct mant as a 1.23 fixed point number, and exp as an
    // exponent, such that x = 2^exp * mant
    {
        unsigned one = 0x800000;
        exp = 0;

        //printf("original: u=%08x\n", u);
        if (u > one) {
            do {
                u = u>>1;
                ++exp;
            } while (u > one);
        } else if (u < one) {
            do {
                u = u<<1;
                --exp;
            } while (u < one);
        }
        mant = u;
        exp += (23-16);
    }
    
#else    
    exp = (u >> 23) & 0xff;
    mant = u & 0x7fffff;
    if (exp == 0xff) {
        return mant ? __builtin_inf() : __builtin_nan("");
    }
    if (exp != 0) {
        mant |= 0x800000;
        exp -= 0x7f;
    } else {
        if (mant == 0) {
            return -__builtin_inf();
        }
        exp = -126;
        while (0 == (mant & 0x800000)) {
            mant = mant<<1;
            exp++;
        }
    }
#endif
    r = __builtin_qlog(mant);
    // at this point r is a 5.27 fixed point number giving log2(mant)
    // note that mant was 1.23 by construction, so the upper 5 bits
    // is "24", i.e. we have 0xbnnnnnn
    //__builtin_printf("... mant=0x%08x r=0x%08x exp=%d\n", mant, r, exp);

#ifdef __fixedreal__
    r = (r>>(27-16)-1) & 0x1ffff;
    r = (r+1)>>1; // round
    r += exp<<16;
    return __asfloat(r);
#else    
    // convert back to float
    r &= 0x7ffffff;
    r = (r+8)>>4;
    r += ((127)<<23);
    --exp;
    
    //printf("... r=%f exp=%f\n", __asfloat(r), (float)exp);
    // x = 2^exp * mant
    // so log(x) = exp + log(mant)
    return __asfloat(r) + (float)exp;
#endif    
}

// calculate 2^x
#ifdef __fixedreal__
float __builtin_exp2f(float x)
{
    int n;
    unsigned u, r;
    float y;
    n = (int)x;
    if (n < -16) {
        return 0;
    }
    if (n >= 16) {
        return __asfloat(0x7fffffff);
    }
    y = x - (float)n;
    if (y < 0) {
        y += 1.0f;
        --n;
    }
    u = __asuint(y); // 16.16
    //printf("... u=%08x\n", u);
    u = u<<11;       // 5.27
    u |= (16<<27);
    r = __builtin_qexp(u); // as a 16.16 number
    //printf("... r=%08x\n", r);

    if (n >= 0) {
        r <<= n;
    } else {
        r >>= -n;
    }
    return __asfloat(r);
}
#else
float __builtin_exp2f(float x)
{
    int n;
    float y;
    unsigned u;
    unsigned r;
    unsigned nf;
    
    if (x >= 127.0) {
        return __builtin_inf();
    }
    if (x < -127.0) {
        return 0.0;
    }
    n = (int)x;
    y = x - (float)n;
    if (y < 0) {
        y += 1.0;
        --n;
    }
    
    nf = (n+0x7f) << 23;
    //printf(" ... x=%f = y (%f) + n (%d)\n", x, y, n);
    //printf(" ... 2^n = %f\n", __asfloat(nf));
    
    // OK, 0 <= y < 1 and x = n + y
    // so 2^x = 2^{n+y} = 2^n * 2^y
    u = (unsigned)(y*(1<<27));
    if (u == 0) {
        float x = __asfloat(nf);
        //printf(" ... returning %f\n", x);
        return x;
    }
    u |= (24<<27);
    //printf("..u=%x (input) ", u);
    r = __builtin_qexp(u);
    //printf("..u=%x r=%x\n", u, r);
    // round:
    r = (r+1)>>1;
    r += (0x7e)<<23;
    //printf("..nf=%f (0x%08x), r=%f (0x%08x)\n", __asfloat(nf), nf, __asfloat(r), r);
    return __asfloat(nf)*__asfloat(r);
}
#endif

//
// calculate x^y
//
float __builtin_powf(float x, float y)
{
    int n;
    float a, b;
    n = (int)y;
    if ((float)n == y) {
        // x^y is just x^n
        return _float_pow_n(1.0, x, n);
    }
    if (x < 0) {
        return __builtin_nan("");
    }
    if (x == 0) {
        return 0;
    }
    // calculate x^y as 2^(log_2(x)*y)
    a = __builtin_log2f(x) * y;
    return __builtin_exp2f(a);
}

//
// calculate log base b of x
//
float __builtin_logbase(float b, float x)
{
    float xx = __builtin_log2f(x);
    float bb = __builtin_log2f(b);

    return xx / bb;
}

//
// various standard functions
//
float __builtin_expf(float x)
{
    return __builtin_powf(CONST_E, x);
}

float __builtin_exp10f(float x)
{
    return __builtin_powf(10.0, x);
}

float __builtin_logf(float x)
{
    return __builtin_logbase(CONST_E, x);
}

float __builtin_log10f(float x)
{
    return __builtin_logbase(10.0, x);
}

