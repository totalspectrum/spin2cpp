/**
 * @file include/limits.h
 * @brief Defines macros for limits and parameters of the standard integer types.
 *
 * The compiler supports data with the following characteristics:
 * @li 8 bit char
 * @li 16 bit short
 * @li 32 bit int
 * @li 32 bit long
 * @li 64 bit long long
 *
 * The compiler supports UTF-8, but does not support multi-byte characters.
 */
#ifndef _LIMITS_H
#define _LIMITS_H

#include <compiler.h>

/*
 * this file is designed for compilers which fit the following
 * characteristics:
 * 8 bit char
 * 16 bit short
 * 32 or 64 bit int (define __INT_SIZE
 * either 32 or 64 bit long (define __LONG_SIZE)
 */

#ifndef __INT_SIZE
/* assume a default of 32 bits, i.e. 4 bytes */
#define __INT_SIZE 4
#endif
#ifndef __LONG_SIZE
/* assume a default of 32 bits, i.e. 4 bytes */
#define __LONG_SIZE 4
#endif
#ifndef _CHAR_IS_UNSIGNED
/* assume signed char as default */
#define _CHAR_IS_UNSIGNED 0
#endif

#define CHAR_BIT    8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  (127)
#define UCHAR_MAX  (255)

#if _CHAR_IS_UNSIGNED
#define CHAR_MIN (0)
#define CHAR_MAX UCHAR_MAX
#else
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX
#endif

/* maximum length of a multibyte character
 * at present we do not support multibyte
 * characters, but we support UTF-8
 */
#define MB_LEN_MAX 4

#define SHRT_MIN (-32768)
#define SHRT_MAX (32767)
#define USHRT_MAX (65535)

#define _INT32_MIN (-2147483648UL)
#define _INT32_MAX (2147483647L)
#define _UINT32_MAX (4294967295UL)

#define _INT64_MIN (-9223372036854775808ULL)
#define _INT64_MAX (9223372036854775807LL)
#define _UINT64_MAX (18446744073709551615ULL)

#if _INT_SIZE == 2
#define INT_MIN SHRT_MIN
#define INT_MAX SHRT_MAX
#define UINT_MAX USHRT_MAX
#elif _INT_SIZE == 4
#define INT_MIN _INT32_MIN
#define INT_MAX _INT32_MAX
#define UINT_MAX _UINT32_MAX
#else
#define INT_MIN _INT64_MIN
#define INT_MAX _INT64_MAX
#define UINT_MAX _UINT64_MAX
#endif

#if _LONG_SIZE == 4
#define LONG_MIN _INT32_MIN
#define LONG_MAX _INT32_MAX
#define ULONG_MAX _UINT32_MAX
#else
#define LONG_MIN _INT64_MIN
#define LONG_MAX _INT64_MAX
#define LONG_MAX _UINT64_MAX
#endif

#define LLONG_MIN _INT64_MIN
#define LLONG_MAX _INT64_MAX
#define ULLONG_MAX _UINT64_MAX


#if !defined(__STRICT_ANSI__)
/* posix extensions to C99 */
#include <sys/syslimits.h>
#endif

#endif
