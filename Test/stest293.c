int x, y;

int getval(int *, int *b) __attribute__(noinline)
{
    return *b;
}

int getval2() {
    return getval(&x, &y);
}
