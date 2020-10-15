//
// floating point support routines
//

// calculate b * a^n
float _float_pow_n(float b, float a, long n)
{
    int invflag;
    float r;
    if (n < 0) {
        invflag = 1;
        n = -n;
        if (n < 0) {
            return 0;
        }
    } else {
        invflag = 0;
    }
    r = 1.0f;
    while (n > 0) {
        if (n & 1) {
            r = r * a;
        }
        n = n>>1;
        a = a*a;
    }
    if (invflag) {
        r = b / r;
    } else if (b != 1.0f) {
        r = b * r;
    }
    return r;
}
