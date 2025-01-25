#ifndef __PROPELLER2__H
#define __PROPELLER2__H

#include <stdint.h>

#ifndef __FLEXC__
/*
 * special cog register names (the actual types are compiler dependent)
 * Not all compilers will necessarily support these
 */

extern volatile uint32_t _IJMP3; 
extern volatile uint32_t _IRET3;
extern volatile uint32_t _IJMP2;
extern volatile uint32_t _IRET2;
extern volatile uint32_t _IJMP1;
extern volatile uint32_t _IRET1;
extern volatile uint32_t _PA;
extern volatile uint32_t _PB;
extern volatile uint32_t _PTRA;
extern volatile uint32_t _PTRB;
extern volatile uint32_t _DIRA;
extern volatile uint32_t _DIRB;
extern volatile uint32_t _OUTA;
extern volatile uint32_t _OUTB;
extern volatile uint32_t _INA;
extern volatile uint32_t _INB;

#endif

#ifdef _PROP1_COMPATIBLE
/*
 * For compatibility with previous programs, where the special register
 * names did not have the leading underscore, we provide the #defines 
 * to allow older programs to work with the new names:
 */
#define DIRA _DIRA
#define INA  _INA
#define OUTA _OUTA
#define DIRB _DIRB
#define INB  _INB
#define OUTB _OUTB
#endif

/*
 * common definitions
 */

#define ANY_COG 0x10

/*
 * common types
 */

// cartesian coordinates
typedef struct _cartesian {
   int32_t x, y;
} cartesian_t;

// polar coordinates
typedef struct _polar {
   uint32_t r, t;
} polar_t;

// 64 bit counter
typedef struct _counter64 {
    uint32_t low, high;
} counter64_t;

/*
 * P2 32 Bit Clock Mode (see macros below to construct)
 *
 *      0000_000e_dddddd_mmmmmmmmmm_pppp_cc_ss
 *
 *   e          = XPLL (0 = PLL Off, 1 = PLL On)
 *   dddddd     = XDIV (0 .. 63, crystal divider => 1 .. 64)
 *   mmmmmmmmmm = XMUL (0 .. 1023, crystal multiplier => 1 .. 1024)
 *   pppp       = XPPP (0 .. 15, see macro below)
 *   cc         = XOSC (0 = OFF, 1 = OSC, 2 = 15pF, 3 = 30pF)
 *   ss         = XSEL (0 = rcfast, 1 = rcslow, 2 = XI, 3 = PLL)
 */

// macro to calculate XPPP (1->15, 2->0, 4->1, 6->2 ... 30->14) ...
#define XPPP(XDIVP) ((((XDIVP)>>1)+15)&0xF)  

// macro to combine XPLL, XDIV, XDIVP, XOSC & XSEL into a 32 bit CLOCKMODE ...
#define CLOCKMODE(XPLL,XDIV,XMUL,XDIVP,XOSC,XSEL) ((XPLL<<24)+((XDIV-1)<<18)+((XMUL-1)<<8)+(XPPP(XDIVP)<<4)+(XOSC<<2)+XSEL) 

// macro to calculate final clock frequency ...
#define CLOCKFREQ(XTALFREQ, XDIV, XMUL, XDIVP) ((XTALFREQ)/(XDIV)*(XMUL)/(XDIVP))

/*
 * pre-defined functions
 */

void      _clkset(uint32_t clkmode, uint32_t clkfreq);
void      _hubset(uint32_t val);
void	  _reboot(void);

/* start PASM code in another COG */
int       _coginit(int cog, void *pgm, void *ptr);
#define _cognew(pgm, ptr) _coginit(ANY_COG, pgm, ptr)

/* start C code in another COG */
#ifdef __FLEXC__
#define _cogstart_cog(cognum, func, arg, stack, size) __builtin_cogstart_cog(cognum, func(arg), stack)
#define _cogstart(func, arg, stack, size)             __builtin_cogstart(func(arg), stack)
#else
int _cogstart(void (*func)(void *), void *arg, void *stack_base, uint32_t stack_size);
#endif

// aliases used by Catalina
#define _cogstart_PASM(cogid, pgm, arg)     _coginit(cogid, pgm, arg)
#define _cogstart_C(func, arg, stack, size) _cogstart(func, arg, stack, size)
#define _cogstart_C_cog(func, arg, stack, size, cog) _cogstart_cog(cog, func, arg, stack, size)

/* stop/check status of COGs */
void      _cogstop(int cog);
int       _cogchk(int cog);
int       _cogid(void);

int       _locknew(void);
void      _lockret(int lock);

int       _locktry(int lock);
int       _lockrel(int lock);
int       _lockchk(int lock);

void      _cogatn(uint32_t mask);
int       _pollatn(void);
int       _waitatn(void);

/* CORDIC instructions */
cartesian_t _rotxy(cartesian_t coord, uint32_t t);
cartesian_t _polxy(polar_t coord);
polar_t     _xypol(cartesian_t coord);

/* miscellaneous operations */
uint32_t  _rnd(void);
//uint32_t  _rev(uint32_t val);   /* like Spin reverse operator */
#define _rev(val) __builtin_propeller_rev((unsigned)val)
int       _clz(uint32_t val);   /* count leading zeros */
int       _encod(uint32_t val); /* Spin encode operator */
uint32_t  _isqrt(uint32_t val); /* Spin integer square root */
uint32_t  _muldiv64(uint32_t m1, uint32_t m2, uint32_t d); /* calculate m1*m2 / d */
uint32_t  _ones(uint32_t n);    /* count one bits set */

/* counter related functions */
uint32_t  _cnt(void);
uint32_t  _cnth(void); /* high 32 bits of CNT, on processors that support it */
counter64_t _cnthl();  /* fetch both together */
uint32_t  _getsec();   /* seconds elapsed */
uint32_t  _getms();    /* get milliseconds elapsed */
uint32_t  _getus();    /* get microseconds elapsed */

uint32_t  _pollcnt(uint32_t tick);
void      _waitcnt(uint32_t tick);

void      _waitx(uint32_t cycles);
void      _waitsec(uint32_t seconds);
void	  _waitms(uint32_t milliseconds);
void	  _waitus(uint32_t microseconds);

/* regular pin I/O */
void      _pinw(int pin, int val);
void      _pinl(int pin);
void      _pinh(int pin);
void      _pinnot(int pin);
void      _pinrnd(int pin);
void      _pinf(int pin);
int       _pinr(int pin);

/* smart pin controls */
void      _wrpin(int pin, uint32_t val);
void      _wxpin(int pin, uint32_t val);
void      _wypin(int pin, uint32_t val);
void      _akpin(int pin);
uint32_t  _rdpin(int pin);
uint32_t  _rqpin(int pin);

/* set up smart pin: WRPIN=mode, WXVAL=xval, WYPIN=YVAL */
void      _pinstart(int pin, uint32_t mode, uint32_t xval, uint32_t yval);
/* turn off smart pin */
void      _pinclear(int pin);

/* access to previously set clock mode and frequency */
extern uint32_t _clockfreq(void);
extern uint32_t _clockmode(void);

#define _clockfreq() (*(uint32_t *)0x14)
#define _clockmode() (*(uint32_t *)0x18)

#endif

