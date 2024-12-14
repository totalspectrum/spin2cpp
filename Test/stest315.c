long long zcheck1(int i, long long y, long long z) {
    return ({ long long t; if (i) t = y; else t = z; t; });
}
long long zcheck2(int i, long long y, long long z) {
    return i ? y : z;
}
long long zcheck3(int i, long long y, long long z) {
    return i ? 0 : z;
}
long long zcheck4(int i, long long y, long long z) {
    return i ? y : 0;
}
