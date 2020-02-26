#ifndef _MATH_H
#define _MATH_H

#pragma once

#ifdef __FLEXC__
#define abs(x) __builtin_abs(x)
#define fabsf(x) __builtin_abs(x)
#define sqrt(x) __builtin_sqrt(x)
#define sqrtf(x) __builtin_sqrt(x)
#define copysign(x, y) __builtin_copysign((x), (y))
#define copysignf(x, y) __builtin_copysign((x), (y))
#define signbit(x) __builtin_signbit(x)
#define ilogb(x) __builtin_ilogb(x)
#define FP_ILOGB0 (-0x7fffffff)
#define FP_ILOGBNAN (0x80000000)

#define scalbnf(x) __builtin_scalbnf(x)

#define exp(x) __builtin_expf(x)
#define log(x) __builtin_logf(x)
#define sin(x) __builtin_sinf(x)
#define cos(x) __builtin_cosf(x)
#define atan(x) __builtin_atanf(x)
#define atan2(y, x) __builtin_atan2f((y), (x))
#define floor(x) __builtin_floorf(x)
#define fabs(x)  __builtin_fabsf(x)
#define frexp(x, p) __builtin_frexpf((x), (p))
#define ldexp(x, n) __builtin_ldexpf((x), (n))
#define modf(x, p) __builtin_modff((x), (p))

#define isinf(x) (__builtin_ilogb(x) == 0x7fffffff)
#define isnan(x) (__builtin_ilogb(x) == FP_ILOGBNAN)

#endif

#endif
