#include <stdio.h>
#include <unistd.h>

size_t fwrite(const void *ptr, size_t elemSize, size_t n, FILE *f)
{
    size_t size = n;

    if (elemSize != 1)
        size *= elemSize;
    if (f->state & (_BUF_FLAGS_READING|_BUF_FLAGS_WRITING)) {
        fflush(f); /* buffer in use, re-sync */
        f->state &= ~(_BUF_FLAGS_READING|_BUF_FLAGS_WRITING);
    }
    size = _vfswrite(f, ptr, size);
    if ((int) size <= 0)
        return 0;
    if (elemSize != 1)
        size /= elemSize;
    return size;
}

size_t fread(void *ptr, size_t elemSize, size_t n, FILE *f)
{
    int r = 0;
    size_t size = n;
    if (elemSize != 1)
        size *= elemSize;
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
    if (f->state & (_BUF_FLAGS_READING|_BUF_FLAGS_WRITING)) {
        fflush(f); /* buffer in use, re-sync */
        f->state &= ~(_BUF_FLAGS_READING|_BUF_FLAGS_WRITING);
    }
    r += _vfsread(f, ptr, size);
#ifdef _DEBUG
    __builtin_printf("vfsread returned %d\n", r);
#endif
    if (r <= 0)
        return 0;
    if (elemSize != 1)
        r /= elemSize;
    return r;
}
