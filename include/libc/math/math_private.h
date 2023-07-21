/*	$OpenBSD: math_private.h,v 1.15 2011/07/26 11:43:01 martynas Exp $	*/
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * from: @(#)fdlibm.h 5.1 93/09/24
 */

#ifndef _MATH_PRIVATE_H_
#define _MATH_PRIVATE_H_

#include <sys/types.h>
#include <stdint.h>

#undef FLT_EVAL_METHOD
#define FLT_EVAL_METHOD (0)

#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234

#define BYTE_ORDER LITTLE_ENDIAN

typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

/* The original fdlibm code used statements like:
	n0 = ((*(int*)&one)>>29)^1;		* index of high word *
	ix0 = *(n0+(int*)&x);			* high word of x *
	ix1 = *((1-n0)+(int*)&x);		* low word of x *
   to dig two 32 bit words out of the 64 bit IEEE floating point
   value.  That is non-ANSI, and, moreover, the gcc instruction
   scheduler gets it wrong.  We instead use the following macros.
   Unlike the original code, we determine the endianness at compile
   time, not at run time; I don't see much benefit to selecting
   endianness at run time.  */

/* A union which permits us to convert between a double and two 32 bit
   ints.  */

/*
 * The arm port is little endian except for the FP word order which is
 * big endian.
 */

#if (BYTE_ORDER == BIG_ENDIAN) || (defined(__arm__) && !defined(__VFP_FP__))

typedef union
{
  double value;
  struct
  {
    u_int32_t msw;
    u_int32_t lsw;
  } parts;
} ieee_double_shape_type;

#endif

#if (BYTE_ORDER == LITTLE_ENDIAN) && !(defined(__arm__) && !defined(__VFP_FP__))

typedef union
{
  double value;
  struct
  {
    u_int32_t lsw;
    u_int32_t msw;
  } parts;
} ieee_double_shape_type;

#endif

/* Get two 32 bit ints from a double.  */

#define EXTRACT_WORDS(ix0,ix1,d)				\
do {								\
  ieee_double_shape_type ew_u;					\
  ew_u.value = (d);						\
  (ix0) = ew_u.parts.msw;					\
  (ix1) = ew_u.parts.lsw;					\
} while (0)

/* Get the more significant 32 bit int from a double.  */

#define GET_HIGH_WORD(i,d)					\
do {								\
  ieee_double_shape_type gh_u;					\
  gh_u.value = (d);						\
  (i) = gh_u.parts.msw;						\
} while (0)

/* Get the less significant 32 bit int from a double.  */

#define GET_LOW_WORD(i,d)					\
do {								\
  ieee_double_shape_type gl_u;					\
  gl_u.value = (d);						\
  (i) = gl_u.parts.lsw;						\
} while (0)

/* Set a double from two 32 bit ints.  */

#define INSERT_WORDS(d,ix0,ix1)					\
do {								\
  ieee_double_shape_type iw_u;					\
  iw_u.parts.msw = (ix0);					\
  iw_u.parts.lsw = (ix1);					\
  (d) = iw_u.value;						\
} while (0)

/* Set the more significant 32 bits of a double from an int.  */

#define SET_HIGH_WORD(d,v)					\
do {								\
  ieee_double_shape_type sh_u;					\
  sh_u.value = (d);						\
  sh_u.parts.msw = (v);						\
  (d) = sh_u.value;						\
} while (0)

/* Set the less significant 32 bits of a double from an int.  */

#define SET_LOW_WORD(d,v)					\
do {								\
  ieee_double_shape_type sl_u;					\
  sl_u.value = (d);						\
  sl_u.parts.lsw = (v);						\
  (d) = sl_u.value;						\
} while (0)

/* A union which permits us to convert between a float and a 32 bit
   int.  */

typedef union
{
  float value;
  u_int32_t word;
} ieee_float_shape_type;

/* Get a 32 bit int from a float.  */

#define GET_FLOAT_WORD(i,d)					\
do {								\
  ieee_float_shape_type gf_u;					\
  gf_u.value = (d);						\
  (i) = gf_u.word;						\
} while (0)

/* Set a float from a 32 bit int.  */

#define SET_FLOAT_WORD(d,i)					\
do {								\
  ieee_float_shape_type sf_u;					\
  sf_u.word = (i);						\
  (d) = sf_u.value;						\
} while (0)

#ifdef FLT_EVAL_METHOD
/*
 * Attempt to get strict C99 semantics for assignment with non-C99 compilers.
 */
#if FLT_EVAL_METHOD == 0 || __GNUC__ == 0
#define	STRICT_ASSIGN(type, lval, rval)	((lval) = (rval))
#else /* FLT_EVAL_METHOD == 0 || __GNUC__ == 0 */
#define	STRICT_ASSIGN(type, lval, rval) do {	\
	volatile type __lval;			\
						\
	if (sizeof(type) >= sizeof(double))	\
		(lval) = (rval);		\
	else {					\
		__lval = (rval);		\
		(lval) = __lval;		\
	}					\
} while (0)
#endif /* FLT_EVAL_METHOD == 0 || __GNUC__ == 0 */
#endif /* FLT_EVAL_METHOD */

/* fdlibm kernel function */
extern int    __ieee754_rem_pio2(double,double*);
extern double __kernel_sin(double,double,int);
extern double __kernel_cos(double,double);
extern double __kernel_tan(double,double,int);
extern int    __kernel_rem_pio2(double*,double*,int,int,int);
extern void   __sincosl(double,double*,double*);

/* float versions of fdlibm kernel functions */
extern int   __ieee754_rem_pio2f(float,float*);
extern float __kernel_sinf(float,float,int);
extern float __kernel_cosf(float,float);
extern float __kernel_tanf(float,float,int);
extern int   __kernel_rem_pio2f(float*,float*,int,int,int,const int*);
extern void  __sincosf(float,float*,float*);

#if !defined(__propeller__)
/* long double precision kernel functions */
long double __kernel_sinl(long double, long double, int);
long double __kernel_cosl(long double, long double);
long double __kernel_tanl(long double, long double, int);
#endif

/*
 * Common routine to process the arguments to nan(), nanf(), and nanl().
 */
void _scan_nan(uint32_t *__words, int __num_words, const char *__s);

/*
 * TRUNC() is a macro that sets the trailing 27 bits in the mantissa
 * of an IEEE double variable to zero.  It must be expression-like
 * for syntactic reasons, and we implement this expression using
 * an inline function instead of a pure macro to avoid depending
 * on the gcc feature of statement-expressions.
 */
#define TRUNC(d)	(_b_trunc(&(d)))

static __inline void
_b_trunc(volatile double *_dp)
{
	uint32_t _lw;

	GET_LOW_WORD(_lw, *_dp);
	SET_LOW_WORD(*_dp, _lw & 0xf8000000);
}

struct Double {
	double	a;
	double	b;
};

/*
 * Functions internal to the math package, yet not static.
 */
double __exp__D(double, double);
struct Double __log__D(double);

#if defined(__SHORT_DOUBLES_IMPL)
/* at runtime functions like sin() will actually be implemented by
 * sinf(), and the double precision functions we're compiling as
 * sin() etc. we will want to be sinl(), etc.
 */
#undef __weak_alias
#define __weak_alias(funcl, func) __asm__("")

#define tgamma tgammal

#define acos acosl
#define acosh acoshl
#define asin asinl
#define atan2 atan2l
#define atanh atanhl
#define cosh coshl
#define exp expl
#define fmod fmodl
#define hypot hypotl
#define log10 log10l
#define log2 log2l
#define log logl
#define pow powl
#define remainder remainderl
#define sinh sinhl
#define sqrt sqrtl
#define asinh asinhl
#define atan atanl

#define cabs cabsl
#define cacos cacosl
#define cacosh cacoshl
#define carg cargl
#define casin casinl
#define casinh casinhl
#define catan catanl
#define catanhl catanhl
#define cbrt cbrtl
#define ccos ccosl
#define ccosh ccoshl
#define ceil ceill
#define cexp cexpl
#define cimag cimagl
#define clog clogl
#define conj conjl
#define copysign copysignl
#define cos cosl
#define cpow cpowl
#define cproj cprojl
#define creal creall
#define csin csinl
#define csinh csinhl
#define csqrt csqrtl
#define ctan ctanl
#define ctanh ctanhl
#define erf erfl
#define erfc erfcl
#define exp2 exp2l
#define expm1 expm1l
#define floor floorl
#define fma fmal
#define fmax fmaxl
#define fmin fminl
#define ilogb ilogbl
#define llrint llrintl
#define llround llroundl
#define log1p log1pl
#define lrint lrintl
#define lround lroundl
#define nan nanl
#define nextafter nextafterl
#define remquo remquol
#define rint rintl
#define round roundl
#define scalbn scalbnl
#define sin sinl
#define tan tanl
#define tanh tanhl
#define trunc truncl
#define lgamma lgammal

#define modf modfl
#define frexp frexpl

#endif

#endif /* _MATH_PRIVATE_H_ */
