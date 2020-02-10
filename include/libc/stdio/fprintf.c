#include <stdio.h>
#include <stdarg.h>
#include <sys/fmt.h>
#undef printf

int printf(const char *fmt, ...)
{
    va_list args;
    int r;
    FILE *f = stdout;

    va_start(args, fmt);
    r = _dofmt( f->putchar, fmt, &args);
    va_end(args);
    return r;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = _dofmt( f->putchar, fmt, &args);
    va_end(args);
    return r;
}

int vprintf(const char *fmt, va_list ap)
{
    int r;
    FILE *f = stdout;
    
    r = _dofmt(f->putchar, fmt, &ap);
    return r;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    int r;
    
    r = _dofmt(f->putchar, fmt, &ap);
    return r;
}
