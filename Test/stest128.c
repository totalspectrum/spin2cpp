int g1;
unsigned g2;

void main()
{
    int i, j;
    j = 2;
    for (i = 4; i != 0; --i) {
        g1 >>= j;
        g2 >>= j;
    }
}
