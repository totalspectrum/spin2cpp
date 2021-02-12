#include <stdio.h>
#include <stdarg.h>
#include <sys/fmt.h>

typedef struct {
    char *ptr;
    char *end;
    int sputc(int c) {
        if (ptr < end) {
            *ptr++ = c;
        }
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

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int r;
    SPInfo S;

    S.ptr = buf;
    S.end = buf + size;
    
    va_start(args, fmt);
    r = _dofmt( &S.sputc, fmt, &args);
    va_end(args);
    // make sure trailing 0 is included
    if (size > 0 && S.ptr == S.end) {
        S.ptr--;
    }
    S.sputc(0);
    return r+1;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    SPInfo S;
    int r;
    
    S.ptr = buf;
    S.end = buf + 0xffffff;
    r = _dofmt(&S.sputc, fmt, &ap);
    S.sputc(0);
    return r;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    SPInfo S;
    int r;
    
    S.ptr = buf;
    S.end = buf + size;
    r = _dofmt(&S.sputc, fmt, &ap);
    // make sure trailing 0 is included
    if (size > 0 && S.ptr == S.end) {
        S.ptr--;
    }
    S.sputc(0);
    return r+1;
}
