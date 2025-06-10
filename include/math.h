#ifndef _MATH_H
#define _MATH_H

#include <compiler.h>

#pragma once

#ifdef __FLEXC__
#define abs(x) __builtin_abs(x)
#define fabsf(x) __builtin_abs(x)
#define sqrt(x) __builtin_sqrt(x)
#define sqrtf(x) __builtin_sqrt(x)
#ifdef __fixedreal__
#define copysign(x, y) __builtin_copysign_fixed((x), (y))
#define copysignf(x, y) __builtin_copysign_fixed((x), (y))
#define ilogb(x) __builtin_ilogb_fixed(x)
#define FP_ILOGB0 (-0x7fffffff)
#define FP_ILOGBNAN (0x80000000)

#define isinf(x) (__builtin_ilogb_fixed(x) == 0x7fffffff)
#define isnan(x) (__builtin_ilogb_fixed(x) == FP_ILOGBNAN)

#else
#define copysign(x, y) __builtin_copysign((x), (y))
#define copysignf(x, y) __builtin_copysign((x), (y))
#define ilogb(x) __builtin_ilogb(x)
#define FP_ILOGB0 (-0x7fffffff)
#define FP_ILOGBNAN (0x80000000)
#define isinf(x) (__builtin_ilogb(x) == 0x7fffffff)
#define isnan(x) (__builtin_ilogb(x) == FP_ILOGBNAN)

#endif

#define signbit(x) __builtin_signbit(x)

#define scalbnf(x) __builtin_scalbnf(x)

#define round(x)  __builtin_round(x)
#define roundf(x) __builtin_round(x)
#define floor(x)  __builtin_floorf(x)
#define floorf(x) __builtin_floorf(x)
#define ceil(x)   __builtin_ceilf(x)
#define ceilf(x)  __builtin_ceilf(x)

#define pow(x, y) __builtin_powf(x, y)
#define powf(x, y) __builtin_powf(x, y)
#define exp(x) __builtin_expf(x)
#define expf(x) __builtin_expf(x)
#define exp2f(x) __builtin_exp2f(x)
#define log(x) __builtin_logf(x)
#define logf(x) __builtin_logf(x)
#define log2(x) __builtin_log2f(x)
#define log2f(x) __builtin_log2f(x)
#define log10(x) __builtin_log10f(x)
#define log10f(x) __builtin_log10f(x)
#define sin(x) __builtin_sinf(x)
#define sinf(x) __builtin_sinf(x)
#define cos(x) __builtin_cosf(x)
#define cosf(x) __builtin_cosf(x)
#define tan(x) __builtin_tanf(x)
#define tanf(x) __builtin_tanf(x)
#define atan(x) __builtin_atanf(x)
#define atanf(x) __builtin_atanf(x)
#define atan2(y, x) __builtin_atan2f((y), (x))
#define atan2f(y, x) __builtin_atan2f((y), (x))
#define asin(x) __builtin_asinf(x)
#define asinf(x) __builtin_asinf(x)
#define acos(x) __builtin_acosf(x)
#define acosf(x) __builtin_acosf(x)
#define fabs(x)  __builtin_fabsf(x)
#define frexpf(x, p) __builtin_frexpf((x), (p))
#define frexp(x, p) __builtin_frexpf((x), (p))
#define ldexpf(x, n) __builtin_ldexpf((x), (n))
#define ldexp(x, n) __builtin_ldexpf((x), (n))
#define modf(x, p) __builtin_modff((x), (p))

#define HUGE_VALF __builtin_inf()
#define HUGE_VAL  __builtin_inf()

#endif

#if FLT_EVAL_METHOD == 0
typedef float float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 1
typedef double float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 2
typedef long double float_t;
typedef long double double_t;
#else
#error FLT_EVAL_METHOD not handled
#endif

float expm1f(float x) _IMPL("libc/math/s_expm1f.c");
float coshf(float x) _IMPL("libc/math/e_coshf.c");
float sinhf(float x) _IMPL("libc/math/e_sinhf.c");
float tanhf(float x) _IMPL("libc/math/s_tanhf.c");
float fmodf(float x, float y) _IMPL("libc/math/e_fmodf.c");

#endif
