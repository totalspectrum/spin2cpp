typedef void (*Funcptr)(int);

void foo(int n)
{
    _OUTB = n;
}

Funcptr a = &foo;

Funcptr get1(void)
{
    return a;
}
