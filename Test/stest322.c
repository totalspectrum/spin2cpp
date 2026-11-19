//
// a struct value passed to a call is built in registers of its own; nothing
// reads them as a block, so they need not stay together, and each call's
// registers may be reused by the next
//
typedef struct triple {
    int a, b, c;
} T;

int use(T t) __attribute__(noinline)
{
    return t.a + t.b + t.c;
}

int sum(int k)
{
    int s = 0;
    s += use((T){k, 1, 2});
    s += use((T){k, 3, 4});
    s += use((T){k, 5, 6});
    s += use((T){k, 7, 8});
    return s;
}
