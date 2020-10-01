struct blah_typ {
    char name[12];
    char *data;
} blah[] = {
    { "abcd", "hello"},
    { "XYZW", "goodbye", },
};

char * foo(int i, int j)
{
    return blah[i].data;
}
