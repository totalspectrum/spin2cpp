//#define _DEBUG

#include <sys/types.h>
#include <sys/vfs.h>
#include <stdio.h>

int __default_flush(vfs_file_t *f)
{
    struct _default_buffer *b = (struct _default_buffer *)f->vfsdata;
    int cnt = b->cnt;
    int r;

#ifdef _DEBUG
    __builtin_printf("default_flush: cnt=%d state=0x%x\n", cnt, f->state);
#endif    
    if ( (b->flags & _BUF_FLAGS_WRITING) ) {
        if (cnt > 0) {
            if (f->state & _VFS_STATE_APPEND) {
                if (f->state & _VFS_STATE_NEEDSEEK) {
                    r = (*f->lseek)(f, 0L, SEEK_END);
                    f->state &= ~_VFS_STATE_NEEDSEEK;
                }
            }
            r = (*f->write)(f, b->bufptr, cnt);
        } else {
            r = 0;
        }
    } else if ( (b->flags & _BUF_FLAGS_READING) && cnt ) {
        // have to seek backwards to skip over the read but not
        // consumed bytes
#ifdef _DEBUG
        __builtin_printf("default_flush: reading: cnt=%d ptr=%x\n", cnt, (unsigned)&b->ptr[0]);
#endif    
        r = (*f->lseek)(f, -cnt, SEEK_CUR);
#ifdef _DEBUG
        __builtin_printf("default_flush: seek returned: cnt=%d\n", r);
#endif    
        if (r >= 0) r = cnt;  // flushed OK
    }
    b->cnt = 0;
    b->ptr = 0;
    b->flags = 0;
    return 0;
}

int __default_filbuf(vfs_file_t *f)
{
    struct _default_buffer *b = (struct _default_buffer *)f->vfsdata;
    int r;

    if (!b->bufsiz) {
        b->bufptr = &b->bufdata[0];
        b->bufsiz = _DEFAULT_BUFSIZ;
    }
    r = (*f->read)(f, b->bufptr, b->bufsiz);
    if (r < 0) {
        return -1;
    }
    b->cnt = r;
    b->ptr = &b->bufptr[0];
    b->flags |= _BUF_FLAGS_READING;
    return r;
}

int __default_putc(int c,  vfs_file_t *f)
{
    struct _default_buffer *b = (struct _default_buffer *)f->vfsdata;
    if (b->flags & _BUF_FLAGS_READING) {
        __default_flush(f);
    }
    b->flags |= _BUF_FLAGS_WRITING;
    int i = b->cnt;
#ifdef _DEBUG_EXTRA
    __builtin_printf("putc: %d f=%x b=%x cnt=%d\n", c, (unsigned)f, (unsigned)b, i);
#endif
    b->bufptr[i++] = c;
    c &= 0xff;
    b->cnt = i;
    unsigned mode = f->bufmode;
    if ( mode == _IONBF || i == b->bufsiz || (c == '\n' && mode == _IOLBF)) {
        if (__default_flush(f)) {
            c = -1;
        }
    }
    return c;
}

int __default_getc(vfs_file_t *f) {
    struct _default_buffer *b = (struct _default_buffer *)f->vfsdata;
    if (b->flags & _BUF_FLAGS_WRITING) {
        __default_flush(f);
    }
    b->flags |= _BUF_FLAGS_READING;
    int i = b->cnt;
    unsigned char *ptr;

#ifdef _DEBUG_EXTRA
    __builtin_printf("getc: %d\n", i);
#endif    
    if (i == 0) {
        i = __default_filbuf(f);
#ifdef _DEBUG_EXTRA  
        __builtin_printf("filbuf: %d\n", i);
#endif        
    }
    if (i <= 0) {
        return -1;
    }
    b->cnt = i-1;
    ptr = b->ptr;
    i = *ptr++;
    b->ptr = ptr;
    return i;
}

int __default_buffer_init(vfs_file_t *f)
{
    struct _default_buffer *b = (struct _default_buffer *)f->vfsdata;

    if (_isatty(f)) {
        f->bufmode = _IOLBF | _IOBUF;
    } else {
        f->bufmode = _IOFBF | _IOBUF;
    }
    b->bufptr = &b->bufdata[0];
    b->bufsiz = _DEFAULT_BUFSIZ;
    return 0;
}
