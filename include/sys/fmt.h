#include <stdint.h>

#pragma once

#ifdef SMALL_INT
#define UITYPE uint32_t
#define ITYPE  int32_t
#define FTYPE float
#else
#define UITYPE uint64_t
#define ITYPE  int64_t
#define FTYPE double
#endif
#define BITCOUNT (8*sizeof(UITYPE))

//
// string formatting functions
//
typedef int (*putfunc)(int c);
#define CALL(fn, c) (*fn)(c)

//
// flags:
//  8 bits maximum width
//  8 bits minimum width
//  6 bits precision
//  2 bits padding character (0=none, 1=space, 2='0', 3=reserved)
//  2 bits justification (0,1=left, 2=right, 3=center)
//  2 bits sign character (0=none, 1='+', 2=' ', 3=unsigned)
//  1 bit alt (for alternate form)
//  1 bit for upper case
//
#define MAXWIDTH_BIT (0)
#define WIDTH_MASK   0xff
#define MINWIDTH_BIT (8)
#define PREC_BIT     (16)
#define PREC_MASK    0x3f
#define JUSTIFY_BIT  (22)
#define JUSTIFY_MASK 0x3
#define PADCHAR_BIT  (24)
#define PADCHAR_MASK 0x3
#define SIGNCHAR_BIT (26)
#define SIGNCHAR_MASK 0x3
#define ALTFMT_BIT   (28)
#define UPCASE_BIT   (29)

#define JUSTIFY_LEFT  1
#define JUSTIFY_RIGHT 2
#define JUSTIFY_CENTER 3

#define PADCHAR_NONE  0
#define PADCHAR_SPACE 1
#define PADCHAR_ZERO  2

#define SIGNCHAR_NONE 0
#define SIGNCHAR_PLUS 1
#define SIGNCHAR_SPACE 2
#define SIGNCHAR_UNSIGNED 3
#define SIGNCHAR_MINUS 4

#if 0
// already declared in system module
int _fmtpad(putfunc fn, unsigned fmt, int width, unsigned leftright) _IMPL("libsys/fmt.c");
int _fmtstr(putfunc fn, unsigned fmt, const char *str) _IMPL("libsys/fmt.c");
int _fmtchar(putfunc fn, unsigned fmt, int c) _IMPL("libsys/fmt.c");
int _fmtnum(putfunc fn, unsigned fmt, int x, int base) _IMPL("libsys/fmt.c");
int _fmtfloat(putfunc fn, unsigned fmt, FTYPE x, int spec) _IMPL("libsys/fmt.c");
#endif

int _dofmt(putfunc fn, const char *fmtstr, va_list *args) _IMPL("libsys/dofmt.c");

