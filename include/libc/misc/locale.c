/*
 * From the MiNT library
 *
 * Routines for handling the local environment.
 * WARNING: This probably isn't in accord with the ANSI standard yet.
 *
 * Written by Eric R. Smith and placed in the public domain.
 *
 */

#include <stddef.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

/* locale structure */
/* empty strings, or values of CHAR_MAX, indicate the item is not available
 * in this locale
 */

static struct lconv C_locale = {
        ".",    /* decimal point for ordinary numbers */
        "",     /* thousands separator */
        "",     /* how digits in ordinary numbers are grouped */
        "",     /* decimal point for money */
        "",     /* thousands separator for money */
        "",     /* how digits in a monetary value are grouped */
        "",     /* symbol for positive amount of money */
        "",     /* symbol for negative amount of money */
        "",     /* currency symbol for ordinary use */

        CHAR_MAX,      /* local: number of places after '.' for money */
        CHAR_MAX,      /* currency symbol 1 precedes 0 succeeds positive value */
        CHAR_MAX,      /* currency symbol 1 precedes 0 succeeds neg. value */
        CHAR_MAX,      /* 1=space 0=no space between currency symbol and pos. value */
        CHAR_MAX,      /* 1=space 0=no space between currency symbol and neg. value */
        CHAR_MAX,      /* position of sign in postive money values (???) */
        CHAR_MAX,      /* position of sign in negative money values (???) */

	"",         /* currency symbol for international use */

        CHAR_MAX,      /* international: number of places after '.' for money */
        CHAR_MAX,      /* currency symbol 1 precedes 0 succeeds positive value */
        CHAR_MAX,      /* currency symbol 1 precedes 0 succeeds neg. value */
        CHAR_MAX,      /* 1=space 0=no space between currency symbol and pos. value */
        CHAR_MAX,      /* 1=space 0=no space between currency symbol and neg. value */
        CHAR_MAX,      /* position of sign in postive money values (???) */
        CHAR_MAX,       /* position of sign in negative money values (???) */

};

/* current locale info */
static struct lconv *_LC_Curlocale = &C_locale;


/* localeconv: return current locale information */

struct lconv *localeconv(void)
{
        return _LC_Curlocale;
}

/* setlocale: set the locale.
 * We only support two locales:
 * "C" is the ordinary C locale that we start with, and uses ASCII
 *         as its character encoding
 * "C.UTF-8" is the C locale with a UTF-8 encoding
 */

static const char C_UTF8_NAME[] = "C.UTF-8";
static const char C_NAME[] = "C";

char *setlocale(int category, const char *name)
{
  const char *result = NULL;
  if (name) {
    if (!name[0] || !strcmp(name, "C.UTF-8")) {
      result = C_UTF8_NAME;
    } else if (!strcmp(name, C_NAME)) {
      result = C_NAME;
    }
  } else {
    if (category & LC_CTYPE) {
      result = (MB_CUR_MAX == 1) ? C_NAME : C_UTF8_NAME;
    } else {
      result = C_NAME;
    }
  }
  if (!result)
    return (char *)result;

  /* set the locale info here */
  if (category & LC_CTYPE) {
    if (result == C_NAME) {
      /* ASCII conversion */
      MB_CUR_MAX = 1;
      _mbrtowc_ptr = _mbrtowc_ascii;
      _wcrtomb_ptr = _wcrtomb_ascii;
    } else {
      /* UTF-8 conversion */
      MB_CUR_MAX = 4;
      _mbrtowc_ptr = _mbrtowc_utf8;
      _wcrtomb_ptr = _wcrtomb_utf8;
    }
  }
  return (char *)result;  
}
