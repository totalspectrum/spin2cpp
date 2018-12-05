void *mylongset(void *buf, int c, unsigned long n)
{
    int *ptr = (int *)buf;
    for ( ; n > 0; --n) {
        *ptr++ = c;
    }
    return buf;
}
 
