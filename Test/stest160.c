void *g_base;
unsigned long g_size;

void setsize(void *arg, unsigned long size)
{
    g_size = size / sizeof(int);
}
