char *fetchbuf()
{
    static char buf[4];

    buf[0] = _OUTA;
    return buf[0] ? 0 : buf;
}
