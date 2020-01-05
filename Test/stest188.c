int test1(int x, unsigned char y)
{
    return y | (x << 8);
}

int test2(int x, unsigned char y)
{
    return (x << 8) | y;
}
