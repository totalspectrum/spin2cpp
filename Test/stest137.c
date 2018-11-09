char bar[] = { 1, 2, 3 };

int blah(void)
{
    return sizeof(bar);
}

int getbar(int i)
{
    return bar[i];
}
