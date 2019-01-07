/*
 * Copyright 2012 Total Spectrum Software Inc.
 * Redistribution is permitted under the terms of the MIT license
 */

#include <compiler.h>
#include <wchar.h>
#include <errno.h>

/* convert multibyte sequence to single byte; ASCII version */

/* process a single wide character, updating the mbstate;
 * returns:
 * count of bytes consumed if a valid character is seen
 * -1 if an error happens (invalid sequence)
 * -2 if more data remains
 *
 * for ASCII the -2 can happen only if n == 0
 */

size_t
_mbrtowc_ascii(wchar_t *wcptr, const char *cptr, size_t n, _Mbstate_t *ps)
{
  unsigned char c;
  if (!cptr) {
    return 0;
  }
  if (n == 0) {
    return (size_t)-2;
  }
  c = *cptr;
  if (c == 0) {
    return 0;
  } else {
    if (wcptr) *wcptr = c;
    return 1;
  }
}

size_t (*_mbrtowc_ptr)(wchar_t *, const char *, size_t, mbstate_t *) = _mbrtowc_ascii;
