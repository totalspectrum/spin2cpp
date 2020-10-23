/**
 * @file include/compiler.h
 *
 * @brief Defines features of the compiler being used.
 *
 * @details
 * This file defines various features of the compiler being used such as:
 * @li _INT_SIZE   2 for 16 bit, 4 for 32 bit
 * @li _LONG_SIZE  4 for 32 bit, 8 for 64 bit
 * @li _WCHAR_SIZE sizeof(wchar_t)
 * @li _CHAR_IS_UNSIGNED 1 or 0, depending
 * @li _NORETURN: attribute indicating a function does not return
 * @li _CONST:    indicates that the function does not change or examine memory
 * @li _WEAK:     indicates a weak definition that will be overridden by any strong definition
 */

#ifndef _COMPILER_H
#define _COMPILER_H

#pragma once

#if defined(__GNUC__)
#define _INT_SIZE  __SIZEOF_INT__
#define _LONG_SIZE __SIZEOF_LONG__
#define _WCHAR_SIZE __SIZEOF_WCHAR_T__
#define _INLINE inline
#ifdef __CHAR_UNSIGNED__
#define _CHAR_IS_UNSIGNED 1
#else
#define _CHAR_IS_UNSIGNED 0
#endif
#define _NORETURN __attribute__((noreturn))
#define _CONST    __attribute__((const))
#define _WEAK     __attribute__((weak))

#ifndef _WCHAR_T_TYPE
#define _WCHAR_T_TYPE __WCHAR_TYPE__
#endif
#define _CONSTRUCTOR __attribute__((constructor))
#define _DESTRUCTOR __attribute__((destructor))
#ifdef __PROPELLER_CMM__
#define _CACHED     __attribute__((fcache))
#else
#define _CACHED
#endif

#define _NAN __builtin_nan("1")
#define _NANL __builtin_nanl("1")
#define _NANF __builtin_nanf("1")

/*
 * __weak_alias creates a weak alias for a symbol
 * for example, if you have a function "myprintf" and
 * you want it to provide the "printf" function unless
 * somebody explicitly defines printf elsewhere, you would
 * do  __weak_alias(printf, myprintf)
 */
#ifndef __weak_alias
#define __weak_alias(sym, oldfunc) \
  __asm__( " .weak _" #sym "\n  .equ _" #sym ",_" #oldfunc "\n" )
#endif

/*
 * create a strong alias for a symbol
 */
#ifndef __strong_alias
#define __strong_alias(sym, oldfunc) \
  __asm__( " .global _" #sym "\n  .equ _" #sym ",_" #oldfunc "\n" )
#endif

/*
 * create a reference for a symbol
 */
#ifndef __reference
#define __reference(sym) \
  __asm__( " .global _" #sym "\n" )
#endif

#elif defined(__FLEXC__)

#define _INT_SIZE     4
#define _LONG_SIZE    4
#define _WCHAR_SIZE   4
#define _CHAR_IS_UNSIGNED 1
#define _NORETURN
#define _CONST
#define _WEAK
#define _CACHED
#define _INLINE inline

#ifndef _WCHAR_T_TYPE
#define _WCHAR_T_TYPE unsigned char
#endif

#define _IMPL(x) __fromfile(x)

#define FLT_EVAL_METHOD 0

#else

#error "compiler not yet supported"

#endif

#ifndef _IMPL
#define _IMPL(x)
#endif

#endif
