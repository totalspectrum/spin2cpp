#include <stdarg.h>

unsigned long long getit1(unsigned char *p)
{
    unsigned long long x;
    unsigned long long y;
    y = *(unsigned long long *)p;
    p += 8;
    x = y;
    return x;
}

unsigned long long getit2(unsigned char *p)
{
    unsigned long long x;
    unsigned long long y;
    (y = *(unsigned long long *)p, p += 8, x = y);
    return x;
}

unsigned long long getit3(unsigned char *p)
{
    unsigned long long x;
    unsigned long long y;
    x = (y = *(unsigned long long *)p, p += 8, y);
    return x;
}

unsigned long long getit9(va_list args)
{
    unsigned long long x;

    x = va_arg(args, unsigned long long);
    return x;
}

unsigned long long getitA(va_list *args)
{
    unsigned long long x;

    x = va_arg(*args, unsigned long long);
    return x;
}

