int sfetch(signed char *r)
{
    int y = *r;
    return y;
}
int ufetch(unsigned char *r)
{
    int y = *r;
    return y;
}

int ushift(unsigned long x, int y)
{
    return x >> y;
}

int sshift(int x, int y)
{
    return x >> y;
}
