#include <string.h>
#include <stdlib.h>
#include "util.h"

//
// convert an unsigned 64 bit number to ASCII in an an arbitrary base
// "buf" is the output buffer, which must be big enough to
// hold it
//
// "prec" is the minimum number of digits to output
// returns number of digits produced
//

int
lltoa_prec( unsigned long long x, char *buf, unsigned base, int prec )
{
    int digits = 0;
    int c;

    if (prec < 0) prec = 1;

    while (x > 0 || digits < prec) {
        c = x % base;
        x = x / base;
        if (c < 10) c += '0';
        else c = (c - 10) + 'a';
        buf[digits++] = c;
    }
    buf[digits] = 0;
    strrev(buf);
    return digits;
}
