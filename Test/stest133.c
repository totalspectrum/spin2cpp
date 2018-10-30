int foo(const char *s)
{
    int n = 0;
    while (*s) s++,n++;
    return n;
}

int bar()
{
    return foo("hello\r\nthere");
}
