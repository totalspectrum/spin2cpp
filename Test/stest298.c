void set1(int *xPrev, int es) {
    int i;
    for (i = 0; i < 18; i++) {
        *xPrev++ >>= es;
    }
}

void set2(int *xPrev, int es) {
    int i;
    for (i = 0; i < 18; i++) {
        *++xPrev >>= es;
    }
}
