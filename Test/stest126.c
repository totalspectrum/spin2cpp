int
mystrlen(const char *orig_s)
{
    const char *s = orig_s;
    while (s[0] != 0)
        s++;
    return (s - orig_s);
}

void
wordxpand(unsigned long *dst, const unsigned short *src)
{
    int c;
    do {
        c = *src++;
        *dst = c;
        dst = dst + 1;
    } while (c != 0);
}
