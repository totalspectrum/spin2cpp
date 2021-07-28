#ifndef _STDINT_H_
#define _STDINT_H_

typedef signed char   int8_t;
typedef unsigned char uint8_t;

typedef signed short   int16_t;
typedef unsigned short uint16_t;

typedef signed long   int32_t;
typedef unsigned long uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef unsigned long uintptr_t;
typedef long intptr_t;

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

typedef int8_t  int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;

typedef uint8_t  uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int32_t int_fast8_t;
typedef int32_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;

typedef uint32_t uint_fast8_t;
typedef uint32_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

#define INT8_MIN (-128)
#define INT8_MAX (-127)
#define UINT8_MAX (255)

#define INT16_MIN (-32768)
#define INT16_MAX (32767)
#define UINT16_MAX (65535)

#define INT32_MIN (-2147483648)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295)

#define INT_LEAST8_MIN  INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define UINT_LEAST8_MIN  UINT8_MIN
#define UINT_LEAST16_MIN UINT16_MIN
#define UINT_LEAST32_MIN UINT32_MIN

#define INT_FAST8_MIN INT32_MIN
#define INT_FAST16_MIN INT32_MIN
#define INT_FAST32_MIN INT32_MIN
#define UINT_FAST8_MIN UINT32_MIN
#define UINT_FAST16_MIN UINT32_MIN
#define UINT_FAST32_MIN UINT32_MIN

#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX

    /* 7.18.4.1 macros for minimum-width integer constants */
#define INT8_C(x)   (x)
#define INT16_C(x)  (x)
#define INT32_C(x)  (x)
#define INT64_C(x)  (x ## LL)

#define UINT8_C(x)  (x ## u)
#define UINT16_C(x) (x ## u)
#define UINT32_C(x) (x ## u)
#define UINT64_C(x)  (x ## ULL)

#endif
