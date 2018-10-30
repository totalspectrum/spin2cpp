int foo1(int x)
{
    return (unsigned char)x;
}
int foo2(int x)
{
    return (signed char)x;
}
int foo3(int x)
{
    return (unsigned short)(signed char)x;
}
