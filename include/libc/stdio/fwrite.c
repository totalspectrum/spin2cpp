#include <stdio.h>
#include <unistd.h>

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *f)
{
    size *= n;
    return _vfswrite(f, ptr, size);
}

size_t fread(void *ptr, size_t size, size_t n, FILE *f)
{
    size *= n;
    return _vfsread(f, ptr, size);
}
