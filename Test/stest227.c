#if 1
char blah[4] = { 0x11, 0x22, 0x33, 0x44 };

int f1(int i)
{
    return blah[i];
}
#endif

#if 1
char foo[2][3] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

int f3(int i, int j)
{
    return foo[i][j];
}
#endif

#if 1
char bar[2][3] = { { 0x81, 0x82, 0x83 }, { 0x91, 0x92, 0x93} };

int f2(int i, int j)
{
    return bar[i][j];
}
#endif

