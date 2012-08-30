/*
 * Functions for manipulating variable sized memory buffers.
 * These buffers can grow and shrink as the program progresses.
 *
 * Written by Eric R. Smith
 * Copyright (c) 2012 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 */

#include "flexbuf.h"
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_GROWSIZE BUFSIZ

void flexbuf_init(struct flexbuf *fb, size_t growsize)
{
    fb->data = NULL;
    fb->len = 0;
    fb->space = 0;
    fb->growsize = growsize ? growsize : DEFAULT_GROWSIZE;
}

size_t flexbuf_curlen(struct flexbuf *fb)
{
    return fb->len;
}

/* add a single character to a buffer */
char *flexbuf_addchar(struct flexbuf *fb, int c)
{
    size_t newlen = fb->len + 1;

    if (newlen > fb->space) {
        char *newdata;
        newdata = realloc(fb->data, fb->space + fb->growsize);
        if (!newdata) return newdata;
        fb->space += fb->growsize;
        fb->data = newdata;
    }
    fb->data[fb->len] = c;
    fb->len = newlen;
    return fb->data;
}

/* add N characters to a buffer */
char *flexbuf_addmem(struct flexbuf *fb, const char *buf, size_t N)
{
    size_t newlen = fb->len + N;

    if (newlen > fb->space) {
        char *newdata;
        size_t newspace;
        newspace = fb->space + fb->growsize;
        if (newspace < newlen) {
            newspace = newlen + fb->growsize;
        }
        newdata = realloc(fb->data, newspace);
        if (!newdata) return newdata;
        fb->space = newspace;
        fb->data = newdata;
    }
    memcpy(fb->data + fb->len, buf, N);
    fb->len = newlen;
    return fb->data;
}

/* add a string to a buffer */
char *flexbuf_addstr(struct flexbuf *fb, const char *str)
{
    return flexbuf_addmem(fb, str, strlen(str));
}

/* retrieve the pointer for a flexbuf */
/* "peek" gets it but keeps it reserved;
 * "get" gets it and releases it from the flexbuf
 */
char *flexbuf_peek(struct flexbuf *fb)
{
    char *r = fb->data;
    return r;
}

char *flexbuf_get(struct flexbuf *fb)
{
    char *r = fb->data;
    flexbuf_init(fb, fb->growsize);
    return r;
}

/* reset the buffer to empty (but do not free space) */
void flexbuf_clear(struct flexbuf *fb)
{
    fb->len = 0;
}

/* free the memory associated with a buffer */
void flexbuf_delete(struct flexbuf *fb)
{
    if (fb->data)
        free(fb->data);
    flexbuf_init(fb, 1);
}
