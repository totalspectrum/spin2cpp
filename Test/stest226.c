struct blah {
    int x, y;
};

#if 1
struct blah foo = { 1, 2 };

void f1()
{
    OUTA = foo.x;
    OUTB = foo.y;
}
#endif

#if 1
struct blah bar = {
    .y = 4, .x = 3
};

void f2()
{
    OUTA = bar.x;
    OUTB = bar.y;
}
#endif
