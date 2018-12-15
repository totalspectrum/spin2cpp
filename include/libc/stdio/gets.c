/* simple gets() */
/* written by Eric R. Smith and placed in the public domain */

#include <stdio.h>

char *gets(char *data)
{
    char *p = data;
    int c;
    while ( (c = getchar()) != EOF && (c != '\n') && (c != '\r') ) {
        if (c == '\b') {
            if (p > data) {
                --p;
            }
        } else {
            *p++ = c;
        }
    }
    *p = 0;
    if (c == EOF && p == data) {
        return NULL;
    }
    return data;
}
