
void blah(int i)
{
    int x, y;
    x = i + 1;
    y = i - 1;

    _OUTA = x;
    _DIRA = y;
}

void main()
{
    int i;
    for (i = -1; i < 2; i++) {
        blah(i);
    }
}
