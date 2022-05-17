typedef float (*Fptr)(int x, void *p);

Fptr gf;

int testit(Fptr t) {
    gf = (char (*)(int, void *))testit;
}
