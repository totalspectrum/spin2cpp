typedef union {
    char c[6];
    long l;
    float f;
} Multitype;

int siz()
{
    return sizeof(Multitype);
}
