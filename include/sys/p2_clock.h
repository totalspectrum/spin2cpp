#if _XTALFREQ == 20000000

#define _XDIV_20MHZ 1
#define _XDIV_10MHZ 2
#define _XDIV_5MHZ  4
#define _XDIV_4MHZ  5
#define _XDIV_2MHZ  10
#define _XDIV_1MHZ  20

#elif _XTALFREQ == 16000000

#define _XDIV_16MHZ 1
#define _XDIV_8MHZ  2
#define _XDIV_4MHZ  4
#define _XDIV_2MHZ 8
#define _XDIV_1MHZ 16

#elif _XTALFREQ == 12000000

#define _XDIV_12MHZ 1
#define _XDIV_6MHZ  2
#define _XDIV_4MHZ  3
#define _XDIV_3MHZ 4
#define _XDIV_2MHZ 6
#define _XDIV_1MHZ 12

#else

#error unknown _XTALFREQ

#endif

#ifndef _XOSC
#error unknown _XOSC
#endif

#ifdef P2_TARGET_MHZ
#if defined(_XDIV_20MHZ) && (0 == (P2_TARGET_MHZ % 20))

#define _XDIV_BASE _XDIV_20MHZ
#define _XMUL (P2_TARGET_MHZ/20)

#elif defined(_XDIV_16MHZ) && (0 == (P2_TARGET_MHZ % 16))

#define _XDIV_BASE _XDIV_16MHZ
#define _XMUL (P2_TARGET_MHZ/16)

#elif defined(_XDIV_12MHZ) && (0 == (P2_TARGET_MHZ % 12))

#define _XDIV_BASE _XDIV_12MHZ
#define _XMUL (P2_TARGET_MHZ/12)

#elif defined(_XDIV_10MHZ) && (0 == (P2_TARGET_MHZ % 10))

#define _XDIV_BASE _XDIV_10MHZ
#define _XMUL (P2_TARGET_MHZ/10)

#elif defined(_XDIV_8MHZ) && (0 == (P2_TARGET_MHZ % 8))

#define _XDIV_BASE _XDIV_8MHZ
#define _XMUL (P2_TARGET_MHZ/8)

#elif defined(_XDIV_4MHZ) && (0 == (P2_TARGET_MHZ % 4))

#define _XDIV_BASE _XDIV_4MHZ
#define _XMUL (P2_TARGET_MHZ/4)

#elif defined(_XDIV_3MHZ) && (0 == (P2_TARGET_MHZ % 3))

#define _XDIV_BASE _XDIV_3MHZ
#define _XMUL (P2_TARGET_MHZ/3)

#else

#define _XDIV_BASE _XDIV_1MHZ
#define _XMUL (P2_TARGET_MHZ)

#endif

#if 0 && (_XDIV_BASE > 1) && (0 == _XDIV_BASE % 2)
// this code seems broken...
#define _XDIVP 2
#define _XDIV (_XDIV_BASE/2)
#else
#define _XDIVP 1
#define _XDIV (_XDIV_BASE)
#endif

#define _XPPPP (((_XDIVP>>1) + 15) & 15)
#define _CLOCKFREQ ((_XTALFREQ / _XDIV) * _XMUL / _XDIVP)
#define _SETFREQ ((1<<24) + ((_XDIV-1)<<18) + ((_XMUL-1)<<8) + (_XPPPP<<4) + (_XOSC<<2))

#endif
