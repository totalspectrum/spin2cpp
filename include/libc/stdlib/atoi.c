/*
 * @atoi.c
 * convert a string to an integer
 *
 * This is a very streamlined and simple converter, understanding
 * only decimal and returning no errors. Use strtol for fancier
 * conversion.
 *
 * Copyright (c) 2011 Parallax, Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 * MIT licensed (see terms at end of file)
 */

#include <stdlib.h>
#include <ctype.h>

long
atol(const char *str)
{
  long num = 0;
  int neg = 0;
  while (isspace(*str)) str++;
  if (*str == '-')
    {
      neg=1;
      str++;
    }
  while (isdigit(*str))
    {
      num = 10*num + (*str - '0');
      str++;
    }
  if (neg)
    num = -num;
  return num;
}

#if defined(__GNUC__) && (_INT_SIZE == _LONG_SIZE)
int atoi(const char *str) __attribute__((alias("atol")));
#else
int atoi(const char *str)
{
  return (int)atol(str);
}
#endif

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
