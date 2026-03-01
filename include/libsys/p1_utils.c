//
// common utilities for P1 (pasm and bytecode)
//
// Copyright 2020,2026 Total Spectrum Software Inc.
// SPDX-License-Identifier: MIT
//

// calculate log base 2 of an unsigned num as a 5.27 fixed point number
// uses the lookup table in ROM
// algorithm comes from Propeller manual
unsigned _qlog(unsigned num)
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
// 
// algorithm comes from Propeller manual
unsigned _qexp(unsigned orig_num)
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
