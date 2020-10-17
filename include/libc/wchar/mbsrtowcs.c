/*
 * Copyright (c) 2011,2012 Parallax Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 * MIT licensed (see terms at end of file)
 */
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <limits.h>

size_t
mbsrtowcs(wchar_t *dest, const char **src_ptr, size_t n, mbstate_t *state)
{
  size_t count, mbn;
  const char *src = *src_ptr;

  if (!dest) n = INT_MAX;
  memset(&state, 0, sizeof(state));
  for (count = 0; count < n; count++) {
    mbn = mbrtowc(dest, src, MB_LEN_MAX, state);
    if (0 > (long)mbn) {
      *src_ptr = src;
      return mbn;
    }
    if (!mbn) {
      *src_ptr = NULL;
      return count;
    }
    if (dest) dest++;
    src += mbn;
  }

  *src_ptr = src;
  return count;
}

/* +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
