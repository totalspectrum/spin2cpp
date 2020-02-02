int blah1()
{
    int x = 0;
    __asm const {
        mov x, #1
        mov x, #2
    };
    return x;
}

int blah2()
{
    int x = 0;
    __asm {
        mov x, #1
        mov x, #2
    };
    return x;
}
