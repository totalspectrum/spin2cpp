struct stuff {
    int x;
    union {
        float f;
        unsigned u;
    } f;
    char s[10];
} blah = { 1, {.f = 1.0}, "hello" };

int getit()
{
    return blah.f.u;
}
