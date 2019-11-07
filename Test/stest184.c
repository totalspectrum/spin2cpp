struct blah {
    int a, b, c, d, e;
} v;

int fetch(struct blah x)
{
    return x.d;
}

int fetchv()
{
    return fetch(v);
}
