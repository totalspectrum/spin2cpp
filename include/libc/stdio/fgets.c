/*
 * @fgets.c
 * Implementation of stdio library functions
 *
 * Copyright (c) 2011 Parallax, Inc.
 * Copyright (c) 2021 Total Spectrum Software Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 * MIT licensed (see terms at end of file)
 */
#include <stdio.h>

char *
fgets(char *buf, int size, FILE *fp)
{
  int c;
  int count = 0;
  int tty = isatty(fileno(fp));
  
  --size;
  while (count < size) {
    c = fgetc(fp);
    if (c < 0) break;
    if (tty) {
        if (c == '\r') {
            c = '\n';
        } else if (c == '\b' || c == 127) {
            if (count > 0) {
                --count;
            }
            continue;
        }
    }
    buf[count++] = c;
    if (c == '\n') break;
  }
  buf[count] = 0;
  return (count > 0) ? buf : NULL;
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
