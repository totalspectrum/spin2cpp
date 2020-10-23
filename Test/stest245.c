struct blah {
    char a[2];
    int b, c;
};

#if 1
struct blah foo[] =
{ 1, 2, 3, 4, 0x11, 0x12, 0x13, 0x14 };
#else
struct blah foo[] =
{ { {1, 2}, 3, 4}, {{0x11, 0x12}, 0x13, 0x14} };
#endif

int get(int i)
{
    return foo[i].b;
}
