/*
 * Implementation of wide character functions
 *
 * Copyright (c) 2012 Parallax, Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 *
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
 */
#include <wctype.h>
#include <string.h>

static struct wcmap {
  const char *str;
  wctype_t ctype;
} wcmap[] = {
  { "alnum", _CTalnum },
  { "alpha", _CTalpha },
  { "blank", _CTb },
  { "cntrl", _CTc },
  { "digit", _CTd },
  { "graph", _CTgraph },
  { "lower", _CTl },
  { "print", _CTprint },
  { "punct", _CTp },
  { "space", _CTs },
  { "upper", _CTu },
  { "xdigit", _CTx },
};

#define NUM_WCTYPES (sizeof(wcmap)/sizeof(wcmap[0]))

wctype_t
wctype(const char *property)
{
  int i;

  for (i = 0; i < NUM_WCTYPES; i++) {
    if (!strcmp(wcmap[i].str, property))
      return wcmap[i].ctype;
  }
  return 0;
}

