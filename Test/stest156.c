typedef struct ibar {
    char *ptr;
    int count;
} PTR;

int x = 1;
PTR bar = { "hello", 2 };
PTR baz = { "aaaa", 3 };

int getbar()
{
    return (int)bar.ptr;
}

int getbaz()
{
    return (int)baz.ptr;
}

void *getbazaddr()
{
    return (void *)&baz;
}
