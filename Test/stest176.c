char *fillbuf(char *orig_str, unsigned num)
{
    char *str = orig_str;
    do {
        *str++ = '1';
        num = num >> 1;
    } while (num > 0);
    *str++ = 0;
    return 0;
}

