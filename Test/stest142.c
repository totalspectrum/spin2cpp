#define va_list unsigned long
#define va_start __builtin_va_start
#define va_arg(vl, typ) __builtin_va_arg(vl, typ)
#define va_end(args)

int
addlist(int n, ...)
{
    va_list v;
    int x;
    int sum = 0;
    va_start(v, n);
    while (n > 0) {
        x = va_arg(v, int);
        sum += x;
    }
    va_end(v);
    return sum;
}

int
get10()
{
    return addlist(4, 1, 2, 3, 4);
}
