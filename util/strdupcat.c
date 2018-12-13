#include <stdlib.h>
#include <string.h>
#include "util.h"

char *strdupcat(const char *a, const char *b)
{
    size_t len = strlen(a) + strlen(b);
    char *c = malloc(len+1);
    strcpy(c, a);
    strcat(c, b);
    return c;
}
