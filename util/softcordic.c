// P2 CORDIC Library
// TODO: anything that isn't QLOG or QEXP, possibly also bit-accurate QDIV overflow
// 2025 Ada Gottensträter
// 

#include "softcordic.h"

#ifdef __propeller2__
#  ifndef FORCE_SOFTCORDIC
#    define P2_HARDCORDIC
#  endif
#endif

#ifdef P2_HARDCORDIC
uint32_t qexp(uint32_t d) {
    uint32_t r;
    __asm {
        qexp d
        getqx r
    }
    return r;
}
uint32_t qlog(uint32_t d) {
    uint32_t r;
    __asm {
        qlog d
        getqx r
    }
    return r;
}
#else

static const int64_t zdelta_logexp[] = { // hyberbolic cordic deltas (official PNut values)
    0x32B803473F,0x179538DEA7,0x0B9A2C912F,0x05C73F7233,0x02E2E683F7,0x01715C285F,0x00B8AB3164,0x005C553C5C,0x002E2A92A3,0x00171547E0,0x000B8AA3C2,0x0005C551DB,0x0002E2A8ED,0x0001715476,0x0000B8AA3B,0x00005C551E,0x00002E2A8F,0x0000171547,0x00000B8AA4,0x000005C552,0x000002E2A9,0x0000017154,0x000000B8AA,0x0000005C55,0x0000002E2B,0x0000001715,0x0000000B8B,0x00000005C5,0x00000002E3,0x0000000171,0x00000000B9
};

#define QLOGEXP_ADJ(N) {x -= x >> N;y -= y >> N;}
#define QEXP_SEC(N) {xd = y>>N, yd = x>>N, zd = zdelta_logexp[N-1]; if (z>=0) {xd=-xd;yd=-yd;zd=-zd;} x-=xd;y-=yd;z+=zd;}
#define QLOG_SEC(N) {xd = y>>N, yd = x>>N, zd = zdelta_logexp[N-1]; if (y<0) {xd=-xd;yd=-yd;zd=-zd;} x-=xd;y-=yd;z+=zd;}

uint32_t qlog(uint32_t d) {
    unsigned mag = d ? __builtin_clz(d) : 31;
    int64_t y = (int64_t)((d << mag)&0x7FFFFFFF)<<6;
    int64_t x = y | 2ull<<37;
    int64_t z = 0;
    int64_t xd,yd,zd;

    QLOG_SEC(1);
    QLOGEXP_ADJ(2); // adjust
    QLOG_SEC(2);
    QLOGEXP_ADJ(3); // adjust
    QLOG_SEC(3);
    QLOGEXP_ADJ(4); // adjust
    QLOG_SEC(4);
    QLOG_SEC(4); // double iteration
    QLOG_SEC(5);
    QLOG_SEC(6);
    QLOGEXP_ADJ(7); // adjust
    QLOG_SEC(7);
    QLOGEXP_ADJ(8); // adjust
    QLOG_SEC(8);
    QLOG_SEC(9);
    QLOGEXP_ADJ(10); // adjust
    QLOG_SEC(10);
    QLOG_SEC(11);
    QLOGEXP_ADJ(12); // adjust
    QLOG_SEC(12);
    QLOG_SEC(13);
    QLOG_SEC(13); // double iteration
    QLOGEXP_ADJ(14); // adjust
    QLOG_SEC(14);
    QLOG_SEC(15);
    QLOGEXP_ADJ(16); // adjust
    QLOG_SEC(16);
    QLOG_SEC(17);
    QLOG_SEC(18);
    QLOGEXP_ADJ(19); // adjust
    QLOG_SEC(19);
    QLOGEXP_ADJ(20); // adjust
    QLOG_SEC(20);
    QLOG_SEC(21);
    QLOGEXP_ADJ(22); // adjust
    QLOG_SEC(22);
    QLOGEXP_ADJ(23); // adjust
    QLOG_SEC(23);
    QLOGEXP_ADJ(24); // adjust
    QLOG_SEC(24);
    QLOGEXP_ADJ(25); // adjust
    QLOG_SEC(25);
    QLOG_SEC(26);
    QLOG_SEC(27);
    QLOG_SEC(28);
    QLOG_SEC(29);
    QLOGEXP_ADJ(30); // adjust
    QLOG_SEC(30);
    QLOG_SEC(31);

    int64_t tmp = ((z>>2)&~0x7Fll)+0x80+((int64_t)(mag^31)<<35);
    if (mag == 0 && tmp&(1ll<<39) == 0){
        tmp = -1;
    }
    return tmp >> 8;
}

uint32_t qexp(uint32_t d) {
    unsigned mag = (d>>27)^31;
    int64_t x = 0x7F42E61C5A; // magic number ??
    int64_t y = 0;
    int64_t z = (int64_t)(d&0x07FFFFFF) << 11;
    int64_t xd,yd,zd;

    QEXP_SEC(1);
    QLOGEXP_ADJ(2); // adjust
    QEXP_SEC(2);
    QLOGEXP_ADJ(3); // adjust
    QEXP_SEC(3);
    QLOGEXP_ADJ(4); // adjust
    QEXP_SEC(4);
    QEXP_SEC(4); // double iteration
    QEXP_SEC(5);
    QEXP_SEC(6);
    QLOGEXP_ADJ(7); // adjust
    QEXP_SEC(7);
    QLOGEXP_ADJ(8); // adjust
    QEXP_SEC(8);
    QEXP_SEC(9);
    QLOGEXP_ADJ(10); // adjust
    QEXP_SEC(10);
    QEXP_SEC(11);
    QLOGEXP_ADJ(12); // adjust
    QEXP_SEC(12);
    QEXP_SEC(13);
    QEXP_SEC(13); // double iteration
    QLOGEXP_ADJ(14); // adjust
    QEXP_SEC(14);
    QEXP_SEC(15);
    QLOGEXP_ADJ(16); // adjust
    QEXP_SEC(16);
    QEXP_SEC(17);
    QEXP_SEC(18);
    QLOGEXP_ADJ(19); // adjust
    QEXP_SEC(19);
    QLOGEXP_ADJ(20); // adjust
    QEXP_SEC(20);
    QEXP_SEC(21);
    QLOGEXP_ADJ(22); // adjust
    QEXP_SEC(22);
    QLOGEXP_ADJ(23); // adjust
    QEXP_SEC(23);
    QLOGEXP_ADJ(24); // adjust
    QEXP_SEC(24);
    QLOGEXP_ADJ(25); // adjust
    QEXP_SEC(25);
    QEXP_SEC(26);
    QEXP_SEC(27);
    QEXP_SEC(28);
    QEXP_SEC(29);
    QLOGEXP_ADJ(30); // adjust
    QEXP_SEC(30);
    QEXP_SEC(31);

    return (((x>>mag)+(y>>mag))+0x40)>>7; // PNut's impl has buggy rounding here (0x20 instead of 0x40)
}
#endif

