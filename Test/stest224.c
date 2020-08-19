
int main()
{
    if (0) {
    here:
        _OUTB = (unsigned)"some big string\n";
    } else {
        _OUTB = (unsigned)"some other string\n";
        goto here;
    }
    return 0;
}
