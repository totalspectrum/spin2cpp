#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

// BASIC support routines
extern int _tx(int c);
extern int _rx(void);

static vfs_file_t __filetab[_MAX_FILES] = {
    /* stdin */
    {
        0, /* vfsdata */
        O_INTRN_RDOK, /* flags */
        0, /* read */
        0, /* write */
        &_tx, /* putchar */
        &_rx, /* getchar */
        0, /* close function */
    },
    /* stdout */
    {
        0, /* vfsdata */
        O_INTRN_WROK, /* flags */
        0, /* read */
        0, /* write */
        &_tx, /* putchar */
        &_rx, /* getchar */
        0, /* close function */
    },
    /* stderr */
    {
        0, /* vfsdata */
        O_INTRN_WROK, /* flags */
        0, /* read */
        0, /* write */
        &_tx, /* putchar */
        &_rx, /* getchar */
        0, /* close function */
    },
};

struct vfs_file_t *
__getftab(int i)
{
    if ( (unsigned)i >= (unsigned)_MAX_FILES) {
        return 0;
    }
    return &__filetab[i];
}

static int
_openas(struct vfs_file_t *fil, const char *name, int flags, mode_t mode)
{
    int r;
    struct vfs *v = _getrootvfs();
    if (!v || !v->open) {
        return _seterror(ENOSYS);
    }
    r = (*v->open)(fil, name, flags);
    if (r < 0 && (flags & O_CREAT)) {
        r = (*v->creat)(fil, name, mode);
    }
    return r;
}

int open(const char *name, int flags, mode_t mode=0644)
{
    struct vfs_file_t *tab = &__filetab[0];
    int fd;
    int r;

    for (fd = 0; fd < _MAX_FILES; fd++) {
        if (tab[fd].flags == 0) break;
    }
    if (fd == _MAX_FILES) {
        return _seterror(EMFILE);
    }
    r = _openas(&tab[fd], name, flags, mode);
    if (r == 0) {
        int rdwr = flags & O_ACCMODE;
        if (rdwr != O_RDONLY) {
            flags |= O_INTRN_WROK;
        }
        if (rdwr != O_WRONLY) {
            flags |= O_INTRN_RDOK;
        }
        tab[fd].flags = flags | O_INUSE;
        r = fd;
    } else {
        /* assume _openas has set errno */
    }
    return r;
}

int close(int fd)
{
    vfs_file_t *f;
    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    if (!f->flags) {
        return _seterror(EBADF);
    }
    (*f->close)(f);
    memset(f, 0, sizeof(*f));
    return 0;
}

ssize_t write(int fd, const void *vbuf, size_t count)
{
    ssize_t r;
    vfs_file_t *f;
    int (*tx)(int c);
    const unsigned char *buf = (const unsigned char *)vbuf;
    
    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    if (! (f->flags & O_INTRN_WROK) ) {
        return _seterror(EACCES);
    }
    if (f->write) {
        r = (*f->write)(f, vbuf, count);
        if (r < 0) {
            return _seterror(r);
        }
        return r;
    }
    tx = f->putchar;
    if (!tx) {
        return _seterror(EACCES);
    }
    r = 0;
    while (count > 0) {
        r += (*tx)(*buf++);
        --count;
    }
    return r;
}

ssize_t read(int fd, void *vbuf, size_t count)
{
    ssize_t r, q;
    vfs_file_t *f;
    int (*rx)();
    unsigned char *buf = (unsigned char *)vbuf;
    
    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    if (! (f->flags & O_INTRN_RDOK) ) {
        return _seterror(EACCES);
    }
    if (f->read) {
        r = (*f->read)(f, vbuf, count);
        if (r < 0) {
            return _seterror(r);
        }
        return r;
    }
    rx = f->getchar;
    if (!rx) {
        return _seterror(EACCES);
    }
    r = 0;
    while (count > 0) {
        q = (*rx)();
        if (q < 0) break;
        *buf++ = q;
        ++r;
        --count;
    }
    return r;
}
