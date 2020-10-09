#define B "b"

char s[] = "a" B "c";

int
fetch(int i)
{
    return s[i];
}
