// calcresult has a small switch statement, so it
// will generate if/then/else instead of a jump table
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
