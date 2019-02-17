/**
 * @file include/ctype.h
 *
 * @brief Provides character class check API macros.
 *
 * @details
 * These functions check whether c, which must have the value of an
 * unsigned char or EOF, falls into a certain character class according
 * to the current locale.
 *
 * @details
 * Conforms to C99, 4.3BSD. C89 specifies all of these functions except isascii() and
 * isblank(). isascii() is a BSD extension and is also an SVr4 extension.
 * isblank() conforms to POSIX.1-2001 and C99 7.4.1.3. POSIX.1-2008 marks
 * isascii() as obsolete, noting that it cannot be used portably in a
 * localized application.
 *
 */
/*
 * doxygen note.
 * \@file uses include/ctype.h to differentiate from include/sys/ctype.h
 * Without this, neither file will be documented.
 */


#ifndef _CTYPE_H
#define _CTYPE_H

#include <compiler.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include <sys/ctype.h>

    int isalnum(int c) _IMPL("libc/misc/ctype.c");
    int isalpha(int c) _IMPL("libc/misc/ctype.c");
    int isblank(int c) _IMPL("libc/misc/ctype.c");
    int iscntrl(int c) _IMPL("libc/misc/iscntrl.c");
    int isdigit(int c) _IMPL("libc/misc/ctype.c");
    int isgraph(int c) _IMPL("libc/misc/isgraph.c");
    int islower(int c) _IMPL("libc/misc/ctype.c");
    int isprint(int c) _IMPL("libc/misc/isprint.c");
    int ispunct(int c) _IMPL("libc/misc/ispunct.c");
    int isspace(int c) _IMPL("libc/misc/ctype.c");
    int isupper(int c) _IMPL("libc/misc/ctype.c");
    int isxdigit(int c) _IMPL("libc/misc/isxdigit.c");

    int tolower(int c) _IMPL("libc/misc/tolower.c");
    int toupper(int c) _IMPL("libc/misc/toupper.c");


#define __isctype(c, x) (__ctype_get(c) & x)

#if !defined(__FLEXC__)    
/**
 * Checks for an alphanumeric class character.
 */
#define isalnum(c)     __isctype(c, _CTalnum)
/**
 * Checks for an alphabetic class character.
 *
 * The isalpha function tests for any character for which isupper or
 * islower is true, or any character that is one of a locale-specific
 * set of alphabetic characters for which none of iscntrl, isdigit,
 * ispunct, or isspace is true.156) In the "C" locale, isalpha returns
 * true only for the characters for which isupper or islower is true.
 */
#define isalpha(c)     __isctype(c, _CTalpha)
/**
 * Checks for a blank (space or tab) character.
 */
#define isblank(c)     __isctype(c, _CTb)
/**
 * Checks for a control character.
 */
#define iscntrl(c)     __isctype(c, _CTc)
/**
 * Checks for a digit (0 through 9) character.
 */
#ifdef __propeller__
/* on the Propeller, this is slightly faster/smaller than the array lookup! */
#define isdigit(c)     (((c)>='0')&&((c)<='9'))
#else
#define isdigit(c)     __isctype(c, _CTd)
#endif

/**
 * Checks for a any printable character except for space.
 */
#define isgraph(c)     (!__isctype(c, (_CTc|_CTs)) && (__ctype_get(c) != 0))
/**
 * Checks for a lower-case character.
 *
 * The islower function tests for any character that is a lowercase
 * letter or is one of a locale-specific set of characters for which
 * none of iscntrl, isdigit, ispunct, or isspace is true. In the * "C"
 * locale, islower returns true only for the characters defined as
 * lowercase letters.
 */
#ifdef __propeller__
#define islower(c)     (((c)>='a')&&((c)<='z'))
#else
#define islower(c)     __isctype(c, _CTl)
#endif
/**
 * Checks for any printable character including space.
 */
#define isprint(c)     (!__isctype(c, (_CTc)) && (__ctype_get(c) != 0))
/**
 * Checks for any printable character that is not a space or alphanumeric character.
 *
 * The ispunct function tests for any printing character that is one of
 * a locale-specific set of punctuation characters for which neither
 * isspace nor isalnum is true.
 */
#define ispunct(c)     __isctype(c, _CTp)
/**
 * Checks for a white space character: space, \\f, \\n, \\r, \\t, \\v in "C" locales.
 */
#define isspace(c)     __isctype(c, _CTs)
/**
 * Checks for an upper-case character.
 *
 * The isupper function tests for any character that is an uppercase
 * letter or is one of a locale-specific set of characters for which
 * none of iscntrl, isdigit, ispunct, or isspace is true. In the "C"
 * locale, isupper returns true only for the characters defined as
 * uppercase letters.
 */
#ifdef __propeller__
#define isupper(c)     (((c)>='A')&&((c)<='Z'))
#else
#define isupper(c)     __isctype(c, _CTu)
#endif
/**
 * Checks for a hexadecimal digit: 0 through 9, a through f, or A through F.
 */
#define isxdigit(c)    __isctype(c, _CTx)
#endif
    
/**
 * Converts an uppercase letter to a corresponding lowercase letter.
 *
 * If the argument is a character for which isupper is true and there
 * are one or more corresponding characters, as specified by the current
 * locale, for which islower is true, the tolower function returns one
 * of the corresponding characters (always the same one for any given
 * locale); otherwise, the argument is returned unchanged.
 *
 * @param c Letter to convert.
 * @returns Uppercase version of c according to locale.
 */
int tolower(int c);

/**
 * Converts a lowercase letter to a corresponding uppercase letter.
 *
 * If the argument is a character for which islower is true and there
 * are one or more corresponding characters, as specified by the current
 * locale, for which isupper is true, the toupper function returns one
 * of the corresponding characters (always the same one for any given
 * locale); otherwise, the argument is returned unchanged.
 *
 * @param c Letter to convert.
 * @returns Lowercase version of c according to locale.
 */
int toupper(int c);

#if defined(__cplusplus)
}
#endif

#endif
