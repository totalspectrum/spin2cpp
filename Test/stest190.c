int foo()
{
    unsigned char n;

    for (n = 80; n; --n ) {
        _OUTA = n;
    }
}
