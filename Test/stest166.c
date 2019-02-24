enum {
    T_NONE,
    T_FIRST=0x100,
    T_SECOND,
    T_THIRD
};

enum {
    S_FOO, S_BAR
};

int get257()
{
    return T_SECOND;
}

int get1()
{
    return S_BAR;
}
