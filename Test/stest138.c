unsigned a[3] = { 0x100, 0x200, 0x300 };

void foo(unsigned *p)
{
    _OUTA = p[0];
}

int
main(void)
{
    foo(a);
}
