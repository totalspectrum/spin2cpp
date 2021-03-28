typedef long long int64_t;
typedef struct { int lo, hi; } mylohi;

int64_t fetch1(int64_t *p)
{
    int64_t r = *p;
    return r;
}

mylohi fetch2(mylohi *p)
{
    mylohi A = *p;
    return A;
}
