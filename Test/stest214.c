char *sum1(char *ptr)
{
    ptr += *(ptr++);
    return ptr;
}
