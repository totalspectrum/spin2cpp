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
#include <compiler.h>

#include "libsimpletext/simpletext.h"

#define print __builtin_printf
#define printi __builtin_printf
#define putChar(x) __builtin_printf("%c", x)
#define putStr(x) __builtin_printf("%s", x)
#define putDec(x) __builtin_printf("%d", x)
#define putHex(x) __builtin_printf("%x", x)

#define pause(ms) _waitms(ms)

#define high(x) _pinh(x)
#define low(x)  _pinl(x)
#define input(x) _pinr(x)
#define toggle(x) _pintoggle(x)
#define get_state(x) _pinr(x)

int random(int limitLow, int limitHigh) _IMPL("libsimpletools/random.c");
unsigned get_states(int endPin, int startPin) _IMPL("libsimpletools/getStates.c");

#endif
