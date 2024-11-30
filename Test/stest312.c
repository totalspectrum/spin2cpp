typedef struct blah {
    int a, b;
} Blah;

int foo1(unsigned short &x) {
    x += 2;
    return x;
}

int foo2(Blah &x) {
    return x.b;
}
