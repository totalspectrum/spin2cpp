#include <stdio.h>

int fflush(FILE *f)
{
    int r = 0;
    if (f->flush) {
        r = (*f->flush)(f);
    }
    return r;
}
