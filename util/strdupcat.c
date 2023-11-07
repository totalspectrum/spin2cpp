#include <stdlib.h>
#include <string.h>
#include "util.h"

char *strdupcat(const char *a, const char *b)
{
    size_t len = strlen(a) + strlen(b);
    char *c = (char *)malloc(len+1);
    strcpy(c, a);
    strcat(c, b);
    return c;
}

char *strndup(const char *src, size_t n)
{
    char *r = (char *)calloc(1, n+1);
    strncpy(r, src, n);
    return r;
}
