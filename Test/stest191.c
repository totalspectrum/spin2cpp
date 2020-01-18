struct blah {
    int a, b;
} u, v;

int fetch(struct blah x)
{
    return x.b;
}

int fetch2(int x, int y)
{
    struct blah B;
    B.a = x;
    B.b = y;
    return fetch(B);
}

int fetchu(void)
{
    return fetch(u);
}

void copy(void)
{
    u = v;
}
