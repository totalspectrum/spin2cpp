/*
 * Copyright (c) 2011 Parallax, Inc.
 * Copyright (c) 2020 Total Spectrum Software Inc.
 * Written by Eric R. Smith, Total Spectrum Software Inc.
 * MIT licensed (see terms at end of file)
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int string_read(FILE *fil, void *bufp, size_t siz)
{
    _STRING_FILE *f = (_STRING_FILE *)fil;
    size_t pos = f->pos;
    size_t len = f->len;
    int r = 0;
    char *buf = (char *)bufp;
    const char *ptr = f->ptr;

    while (pos < len) {
        *buf++ = *ptr++;
        pos++;
        r++;
    }
    f->pos = pos;
    f->ptr = ptr;
    return r;
}

static int string_getc(FILE *fil)
{
    _STRING_FILE *f = (_STRING_FILE *)fil;
    size_t pos = f->pos;
    size_t len = f->len;
    const char *ptr = f->ptr;
    int r;
    if (pos >= len) {
        return -1;
    }
    r = *ptr++;
    pos++;
    f->pos = pos;
    f->ptr = ptr;
    return r;
}

FILE *__string_file(_STRING_FILE *fp, const char *str, const char *mode, size_t len)
{
    memset(fp, 0, sizeof(*fp));
    fp->ptr = str;
    fp->len = len;
    fp->pos = 0;
    fp->file.read = string_read;
    fp->file.getcf = string_getc;
    return &fp->file;
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
