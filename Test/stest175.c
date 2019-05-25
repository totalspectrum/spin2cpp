int calcresult(int sel, int a, int b)
{
    switch(sel) {
    case 1:
        return a+b;
    case -1:
        return a-b;
    default:
        return a;
    }
}
