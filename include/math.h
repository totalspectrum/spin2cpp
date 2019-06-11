#ifndef _MATH_H
#define _MATH_H

#pragma once

#ifdef __FLEXC__
#define sqrt(x) __builtin_sqrt(x)
#define copysign(x, y) __builtin_copysign(x, y)
#define signbit(x) __builtin_signbit(x)
#define ilogb(x) __builtin_ilogb(x)
#define FP_ILOGB0 (-0x7fffffff)
#define FP_ILOGBNAN (0x80000000)

#define isinf(x) (__builtin_ilogb(x) == 0x7fffffff)
#define isnan(x) (__builtin_ilogb(x) == FP_ILOGBNAN)

#endif

#endif
