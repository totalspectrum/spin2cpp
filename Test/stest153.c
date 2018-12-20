struct foo {
    int (*func)(void);
} *B;

int blah()
{
    return B->func();
}
