typedef long long int64_t;

int64_t allbits(int64_t *p, int n)
{
    int64_t r = 0;
    while (n > 0) {
        //r |= *p++;
        r |= *p;
        p++;
        --n;
    }
    return r;
}
