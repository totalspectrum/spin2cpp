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
    return(f->putcf)(c, f);
}

#undef putchar
int putchar(int c) {
    return (stdout->putcf)(c, stdout);
}

int fgetc(FILE *f) {
    int c;
    if (f->ungot) {
        c = f->ungot - 1;
        f->ungot = 0;
    } else {
        c = f->getcf(f);
    }
    return c;
}

int ungetc(int c, FILE *f) {
    if (f->ungot) return -1;
    f->ungot = c+1;
    return c;
}

    
