typedef signed char sbyte;

int fromsbyte(sbyte *x)
{
    return *x;
}

int fromubyte(sbyte *x)
{
    typedef unsigned char ubyte;
    return *(ubyte *)x;
}
