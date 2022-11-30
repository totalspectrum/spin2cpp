unsigned get_high(unsigned long long xx) {
    unsigned r = (xx>>32);
    return r;
}

unsigned long long get_low(unsigned long long x) {
    return x<<32;
}
