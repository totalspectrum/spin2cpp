int foo(int x)
{
    int y = 99;
    switch (x) {
    case 0:
    case 1:
        y = 2;
        break;
    default:
        break;
    }
    return y;
}
