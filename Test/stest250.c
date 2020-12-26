int doresult(int i, int x)
{
    i = i>>24;
    i &= 0x07;
    if ( (i & 2) == 0 ) {
        x++;
    }
    if ( (i & 4) == 0 ) {
        x--;
    }
    return x;
}

       
