typedef struct S {
    int tag;
    union {
        unsigned i;
        float f;
    };
} F_or_D;

float getfloat(F_or_D *u) {
    return u->f;
}
