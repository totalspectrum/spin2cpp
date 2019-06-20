// this calcresult has a big case table, hence
// it should use a jump table
//
// cases:
// -3 => return -a
// -2 => default
// -1 => return a-b
//  0 => default
//  1 => return a+b
//  2 => return a+b
//  3 => return default
//  4 => return b
int calcresult(int sel, int a, int b)
{
    switch(sel) {
    case 2:
    case 1:
        return a+b;
    case -1:
    // case -2 will end up as default
        return a-b;
    case -3:
        return -a;
    case 4:
        return b;
    default:
        return a;
    }
}
