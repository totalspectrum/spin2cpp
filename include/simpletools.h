/*
 * SimpleIDE compatible header file for FlexC
 */

#ifndef SIMPLETOOLS_H_
#define SIMPLETOOLS_H_

#ifdef __propeller2__
#include <propeller2.h>
#else
#include <propeller.h>
#endif

#include "libsimpletext/simpletext.h"

#define print __builtin_printf
#define printi __builtin_printf
#define putStr(x) __builtin_printf("%s", x)
#define putDec(x) __builtin_printf("%d", x)
#define putHex(x) __builtin_printf("%x", x)

#define pause(ms) _waitms(ms)

#define high(x) _pinh(x)
#define low(x)  _pinl(x)

#endif
