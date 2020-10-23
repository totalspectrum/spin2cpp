struct blah_typ {
    int a[2];
    int b[2];
} blah[] = {
    { 1, 2, 3, 4 },
    { {5, 6}, 7, 8 },
    { 9, 10, {11, 12} },
    { {13, 14}, {15, 16}, },
};

int foo(int i, int j)
{
    return blah[i].a[j];
}
