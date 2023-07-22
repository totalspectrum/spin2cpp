#include <sys/types.h>
#include <stdio.h>

int setvbuf(FILE *f, char *buf, int mode, size_t size)
{
    // get the buffer info
    struct _default_buffer *b = (struct _default_buffer *)f->vfsdata;
    if (f->bufmode & _IOBUF) {
        if (!buf) {
            if (size == 0 || mode == _IONBF) {
                buf = &b->bufdata[0];
                size = 1;
            } else {
                buf = malloc(size);
                if (!buf) return -1;
            }
        }
        b->bufsiz = size;
        b->bufptr = buf;
        f->bufmode = _IOBUF | mode;
        return 0;
    }
    return -1; // some kind of problem
}
