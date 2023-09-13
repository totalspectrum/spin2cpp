#include <unistd.h>
#include <stdarg.h>
#include <sys/argv.h>
#ifndef NULL
#define NULL (0)
#endif

static int _execlv(const char *path, const char *arg0, va_list args)
{
    static const char *argv[MAX_ARGC];
    int argc = 0;

    do {
        argv[argc++] = arg0;
        arg0 = va_arg(args, const char *);
    } while (arg0 && argc < MAX_ARGC);
    return _execve(path, argv, NULL);
}

int _execl(const char *path, const char *arg0, ...)
{
    int r;
    va_list args;
    va_start(args, arg0);
    r = _execlv(path, arg0, args);
    va_end(args);
    return r;
}
