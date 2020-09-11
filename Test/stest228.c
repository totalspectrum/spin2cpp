typedef union fi {
    short i;
    float f;
} FI;

FI x = { 1.0 };

int f1() {
    return x.i;
}

FI y = { .f = 1.0 };

int f2() {
    return y.i;
}
