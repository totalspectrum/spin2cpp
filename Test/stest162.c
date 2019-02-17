int blah(int x, int y)
{
    switch (x) {
    case 99:
        if (y < 0) {
            break;
        }
    default:
        /* fall through */
        y = 0;
        break;
    }
    return y;
}

           
