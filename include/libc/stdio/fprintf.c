#include <stdio.h>
#include <stdarg.h>
#include <sys/fmt.h>
#undef printf
#undef putchar

typedef struct _fmtfile {
    FILE *f;
    int putchar(int c) {
        return f->putcf(c, f);
    }
} _FmtFile;

int printf(const char *fmt, ...)
{
    va_list args;
    int r;
    _FmtFile ff;

    ff.f = stdout;

    va_start(args, fmt);
    r = _dofmt( &ff.putchar, fmt, &args);
    va_end(args);
    return r;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list args;
    int r;
    _FmtFile ff;

    ff.f = f;
    va_start(args, fmt);
    r = _dofmt( &ff.putchar, fmt, &args);
    va_end(args);
    return r;
}

int vprintf(const char *fmt, va_list ap)
{
    int r;
    _FmtFile ff;

    ff.f = stdout;
    
    r = _dofmt(&ff.putchar, fmt, &ap);
    return r;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    int r;
    _FmtFile ff;

    ff.f = f;
    r = _dofmt(&ff.putchar, fmt, &ap);
    return r;
}
