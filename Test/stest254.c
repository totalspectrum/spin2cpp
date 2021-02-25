char buf[2][3] = { "01", "ab" };

char *foo(int x)
{
    return &buf[x][0];
}
