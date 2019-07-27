#include <stdio.h>
#include <stdarg.h>
#include <sys/fmt.h>

typedef struct {
    char *ptr;
    char *end;
    int sputc(int c) {
        if (ptr == end) return -1;
        *ptr++ = c;
        return 1;
    }
} SPInfo;

int sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    int r;
    SPInfo S;

    S.ptr = buf;
    S.end = buf + 0xffffff;
    
    va_start(args, fmt);
    r = _dofmt( &S.sputc, fmt, &args);
    va_end(args);
    S.sputc(0);
    return r;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    SPInfo S;

    S.ptr = buf;
    S.end = buf + 0xffffff;
    r = _dofmt(&S.putc, fmt, &ap);
    S.sputc(0);
    return r;
}
