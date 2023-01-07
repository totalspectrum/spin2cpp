#include <stdio.h>
#include <unistd.h>

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *f)
{
    size *= n;
    fflush(f);
    return _vfswrite(f, ptr, size);
}

size_t fread(void *ptr, size_t size, size_t n, FILE *f)
{
    int r = 0;
    size *= n;
    /*
     * we have to handle any already-buffered data
     * on a terminal this could be tricky
     * just punt for now
     */
    if ((f->state & _VFS_STATE_ISATTY)) {
        unsigned char *dst = (unsigned char *)ptr;
        int c;
        while (size > 0) {
            c = fgetc(f);
            if (c < 0) break;
            *dst++ = c;
            r++;
        }
        return r;
    }
    if (f->ungot && size) {
        unsigned char *dst = (unsigned char *)ptr;
        *dst++ = fgetc(f);
        --size;
        r++;
        ptr = (void *)dst;
    }
    if (size == 0) return r;
    fflush(f); /* re-sync */
    r += _vfsread(f, ptr, size);
#ifdef _DEBUG
    __builtin_printf("vfsread returned %d\n", r);
#endif    
    return r;
}
