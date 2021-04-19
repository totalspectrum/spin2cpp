/*
 * @memmove.c
 * Implementation of string library functions
 *
 * Copyright (c) 2011 Parallax, Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 * MIT licensed (see terms at end of file)
 */
#include <string.h>

#undef memmove

/*
 * this function has to handle overlapping regions correctly
 */

void *
memmove(void *dest_p, const void *src_p, size_t n)
{
  void *orig_dest = dest_p;

  const char *src = src_p;
  char *dst = dest_p;

  if ( (unsigned long)dst < (unsigned long)src )
    {
      while (n > 0) {
	*dst++ = *src++;
	--n;
      }
    }
  else
    {
      /* copy backwards */
      dst += n;
      src += n;
      while (n > 0) {
	--n;
	--src;
	--dst;
	*dst = *src;
      }
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
