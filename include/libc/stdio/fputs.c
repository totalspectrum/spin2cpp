#include <stdio.h>

int fputs(const char *s, FILE *f)
{
    int c;
    int q = 0;
    int r;
    while ( (c = *s++) != 0 ) {
        r = fputc(c, f);
        if (r < 0) return r;
        q++;
    }
    return q;
}

int puts(const char *s)
{
    FILE *f = stdout;
    int c;
    int q = 0;
    int r;
    while ( (c = *s++) != 0 ) {
        r = fputc(c, f);
        if (r < 0) return r;
        q++;
    }
    r = fputc('\n', f);
    if (r < 0) return r;
    q++;
    return q;
}

int fputc(int c, FILE *f) {
    return putc(c, f);
}

int fgetc(FILE *f) {
    return getc(f);
}
