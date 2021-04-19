/*
 * @memset.c
 * Implementation of string library functions
 *
 * Copyright (c) 2011 Parallax, Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 * MIT licensed (see terms at end of file)
 */
#include <string.h>
#include <stdint.h>
#include <compiler.h>

#undef memset

/*
 * set some memory to a value
 */
#define ALIGNED(a) ( 0 == ( ((sizeof(uint32_t))-1) & ((unsigned)a) ) )

_CACHED
void *
memset(void *dest_p, int c, size_t n)
{
  void *orig_dest = dest_p;
  char *dst;

  /* fill with longs if applicable */
  if (ALIGNED(dest_p) && n > sizeof(uint32_t))
    {
      uint32_t lc;
      uint32_t *dstl = dest_p;
      c &= 0xff;
      lc = (c<<24)|(c<<16)|(c<<8)|c;
      while (n >= sizeof(uint32_t))
	{
	  *dstl++ = lc;
	  n -= sizeof(uint32_t);
	}
      dest_p = dstl;
    }

  dst = dest_p;
  while (n > 0) {
    *dst++ = c;
    --n;
  }

  return orig_dest;
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
