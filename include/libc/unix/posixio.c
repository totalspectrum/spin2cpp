#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

//#define _DEBUG

static int _rxtxioctl(vfs_file_t *f, unsigned long req, void *argp)
{
    unsigned long *argl = (unsigned long *)argp;
    switch (req) {
    case TTYIOCTLGETFLAGS:
        *argl = _getrxtxflags();
        return 0;
    case TTYIOCTLSETFLAGS:
        _setrxtxflags(*argl);
        return 0;
    default:
        return _seterror(EINVAL);
    }
}

static int __dummy_flush(vfs_file_t *f)
{
    return 0;
}

// BASIC support routines
extern int _tx(int c);
extern int _rx(void);

#ifdef __OUTPUT_BYTECODE__
// need to exactly match up arguments for putc
static int _txputc(int c, void *arg) { _tx(c); return 1; }
static int _rxgetc(void *arg) { return _rx(); }
#else
#define _txputc _tx
#define _rxgetc _rx
#endif

static vfs_file_t __filetab[_MAX_FILES] = {
    /* stdin */
    {
        0, /* vfsdata */
        O_RDONLY, /* flags */
        _VFS_STATE_INUSE|_VFS_STATE_RDOK, /* state */
        0, /* lock */
        0, /* ungot */
        0, /* read */
        0, /* write */
        (putcfunc_t)&_txputc, /* putc */
        (getcfunc_t)&_rxgetc, /* getc */
        0, /* close function */
        &_rxtxioctl,
        &__dummy_flush, /* flush function */
    },
    /* stdout */
    {
        0, /* vfsdata */
        O_WRONLY, /* flags */
        _VFS_STATE_INUSE|_VFS_STATE_WROK,
        0, /* lock */
        0, /* ungot */
        0, /* read */
        0, /* write */
        (putcfunc_t)&_txputc, /* putchar */
        (getcfunc_t)&_rxgetc, /* getchar */
        0, /* close function */
        &_rxtxioctl,
        &__dummy_flush, /* flush function */
    },
    /* stderr */
    {
        0, /* vfsdata */
        O_WRONLY, /* flags */
        _VFS_STATE_INUSE|_VFS_STATE_WROK,
        0, /* lock */
        0, /* ungot */
        0, /* read */
        0, /* write */
        (putcfunc_t)&_txputc, /* putchar */
        (getcfunc_t)&_rxgetc, /* getchar */
        0, /* close function */
        &_rxtxioctl,
        &__dummy_flush, /* flush function */
    },
};

vfs_file_t *
__getftab(int i)
{
    if ( (unsigned)i >= (unsigned)_MAX_FILES) {
        return 0;
    }
    return &__filetab[i];
}

int
_openraw(void *fil_ptr, const char *orig_name, int flags, mode_t mode)
{
    int r;
    struct vfs *v;
    unsigned state = _VFS_STATE_INUSE;
    vfs_file_t *fil = fil_ptr;
    
    char *name = __getfilebuffer();
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->open) {
#ifdef _DEBUG
        __builtin_printf("ENOSYS: vfs == %x\n", (unsigned)v);
#endif        
        return _seterror(ENOSYS);
    }
    memset(fil, 0, sizeof(*fil));
#ifdef _DEBUG
    {
        unsigned *ptr = (unsigned *)v->open;
        __builtin_printf("_openraw: calling %x : %x\n", ptr[0], ptr[1]);
    }
#endif        
    r = (*v->open)(fil, name, flags);
    if (r != 0 && (flags & O_CREAT)) {
#ifdef _DEBUG
        __builtin_printf("_openraw: calling v->creat\n");
#endif
        r = (*v->creat)(fil, name, mode);
    }
#ifdef _DEBUG
    __builtin_printf("_openraw(%s) flags=%d returned %d\n", name, flags, r);
#endif    
    if (r == 0) {
        int rdwr = flags & O_ACCMODE;
        if (rdwr != O_RDONLY) {
            state |= _VFS_STATE_WROK;
        }
        if (rdwr != O_WRONLY) {
            state |= _VFS_STATE_RDOK;
        }
        if (flags & O_APPEND) {
            state |= (_VFS_STATE_APPEND|_VFS_STATE_NEEDSEEK);
        }
#ifdef _DEBUG
        __builtin_printf("openraw rdwr=%d state=%d\n", rdwr, state);
#endif    
        fil->state = state;

        if (!fil->read) fil->read = v->read;
        if (!fil->write) fil->write = v->write;
        if (!fil->close) fil->close = v->close;
        if (!fil->ioctl) fil->ioctl = v->ioctl;
        if (!fil->lseek) fil->lseek = v->lseek;
        if (!fil->putcf) {
            // check for TTY
            int ttychk;
            unsigned int ttyval;
            ttychk = (*fil->ioctl)(fil, TTYIOCTLGETFLAGS, &ttyval);
            if (ttychk == 0 && (ttyval & TTY_FLAG_CRNL)) {
                fil->putcf = &__default_putc_terminal;
            } else {
                fil->putcf = &__default_putc;
            }
#ifdef _DEBUG
            {
                unsigned *ptr = (unsigned *)fil->putcf;
                __builtin_printf("openraw: using default putc (%x: %x %x)\n", (unsigned)ptr, ptr[0], ptr[1]);
            }
#endif                
        }
        if (!fil->getcf) {
            fil->getcf = &__default_getc;
#ifdef _DEBUG
            {
                unsigned *ptr = (unsigned *)fil->getcf;
                __builtin_printf("openraw: using default getc (%x: %x %x)\n", (unsigned)ptr, ptr[0], ptr[1]);
            }
#endif                
        }
        if (!fil->flush) {
            if (v->flush) {
#ifdef _DEBUG
                __builtin_printf("openraw: using vfs flush\n");
#endif                
                fil->flush = v->flush;
            } else {
#ifdef _DEBUG
                __builtin_printf("openraw: using default flush\n");
#endif                
                fil->flush = &__default_flush;
            }
        }
    }
#ifdef _DEBUG
    __builtin_printf("openraw: fil=%x vfsdata=%x\n", (unsigned)fil, (unsigned)fil->vfsdata);
#endif    
    if (r == 0) _seterror(EOK);
    return r;
}

int _closeraw(void *f_ptr)
{
    vfs_file_t *f = f_ptr;
    int r = 0;
    if (!f->state) {
        return _seterror(EBADF);
    }
    if (f->flush) {
        (*f->flush)(f);
    }
    if (f->close) {
        r = (*f->close)(f);
    }
    memset(f, 0, sizeof(*f));
    return r;
}

int _freefile()
{
    vfs_file_t *tab = &__filetab[0];
    int fd;
    for (fd = 0; fd < _MAX_FILES; fd++) {
        if (tab[fd].state == 0) return fd;
    }
    return -1;
}

int open(const char *orig_name, int flags, mode_t mode=0644)
{
    vfs_file_t *tab = &__filetab[0];
    int fd;
    int r;
    
    for (fd = 0; fd < _MAX_FILES; fd++) {
        if (tab[fd].state == 0) break;
    }
    if (fd == _MAX_FILES) {
        return _seterror(EMFILE);
    }
    r = _openraw(&tab[fd], orig_name, flags, mode);
    if (r == 0) {
        r = fd;
    }
    return r;
}

int creat(const char *name, mode_t mode)
{
    return open(name, O_WRONLY|O_CREAT|O_TRUNC, mode);
}

int close(int fd)
{
    vfs_file_t *f;
    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    return _closeraw(f);
}

ssize_t _vfswrite(vfs_file_t *f, const void *vbuf, size_t count)
{
    ssize_t r;
    putcfunc_t tx;
    const unsigned char *buf = (const unsigned char *)vbuf;
    if (! (f->state & _VFS_STATE_WROK) ) {
        return _seterror(EACCES);
    }
    if (f->state & _VFS_STATE_APPEND) {
        if (f->state & _VFS_STATE_NEEDSEEK) {
            r = (*f->lseek)(f, 0L, SEEK_END);
            f->state &= ~_VFS_STATE_NEEDSEEK;
        }
    }
    if (f->write) {
        r = (*f->write)(f, vbuf, count);
        if (r < 0) {
            f->state |= _VFS_STATE_ERR;
            return _seterror(r);
        }
        return r;
    }
    tx = f->putcf;
    if (!tx) {
        return _seterror(EACCES);
    }
    r = 0;
    while (count > 0) {
        r += (*tx)(*buf++, f);
        --count;
    }
    return r;
}

ssize_t write(int fd, const void *vbuf, size_t count)
{
    vfs_file_t *f;
    
    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    return _vfswrite(f, vbuf, count);
}

ssize_t _vfsread(vfs_file_t *f, void *vbuf, size_t count)
{
    ssize_t r, q;
    getcfunc_t rx;
    unsigned char *buf = (unsigned char *)vbuf;
    int break_on_nl = 0;
    
    if (! (f->state & _VFS_STATE_RDOK) ) {
#ifdef _DEBUG
        __builtin_printf("not OK to read\n");
#endif        
        return _seterror(EACCES);
    }
    if (f->read) {
        r = (*f->read)(f, vbuf, count);
#ifdef _DEBUG
        __builtin_printf("f->read(%d) returned %d\n", count, r);
#endif        
        if (r < 0) {
            f->state |= _VFS_STATE_ERR;
            return _seterror(r);
        }
        return r;
    }
#ifdef _DEBUG
    __builtin_printf("f->read(%d) slow path\n", count);
#endif        
    rx = f->getcf;
    if (!rx) {
        return _seterror(EACCES);
    }
    if (f->ioctl) {
        unsigned long val;
        r = (*f->ioctl)(f, TTYIOCTLGETFLAGS, &val);
        if (r == 0 && (val & TTY_FLAG_CRNL)) {
            break_on_nl = 1;
        }
    }
    r = 0;
    while (count > 0) {
        q = (*rx)(f);
        if (q < 0) break;
        *buf++ = q;
        ++r;
        --count;
        if (break_on_nl && q == 10) {
            break;
        }
    }
    return r;
}

ssize_t read(int fd, void *vbuf, size_t count)
{
    vfs_file_t *f;

    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    return _vfsread(f, vbuf, count);
}

off_t lseek(int fd, off_t offset, int whence)
{
    vfs_file_t *f;
    off_t r;
    
    if ((unsigned)fd >= (unsigned)_MAX_FILES) {
        return _seterror(EBADF);
    }
    f = &__filetab[fd];
    if (!f->lseek) {
        return _seterror(ENOSYS);
    }
    if (f->state & _VFS_STATE_APPEND) {
        // if we want to write again, make sure to seek to end of file
        f->state |= _VFS_STATE_NEEDSEEK;
    }
    r = (*f->lseek)(f, offset, whence);
    if (r < 0) {
        return _seterror(-r);
    }
    return r;
}

int
chmod(const char *pathname, mode_t mode)
{
    return _seterror(ENOSYS);
}

int
chown(const char *pathname, uid_t owner, gid_t group)
{
    return _seterror(ENOSYS);
}

int
unlink(const char *orig_name)
{
    int r;
    struct vfs *v;
    char *name = __getfilebuffer();
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->open) {
#ifdef _DEBUG
        __builtin_printf("rmdir: ENOSYS: vfs=%x\n", (unsigned)v);
#endif        
        return _seterror(ENOSYS);
    }
    r = (*v->remove)(name);
    if (r != 0) {
        return _seterror(-r);
    }
    return r;
}

int
rmdir(const char *orig_name)
{
    struct vfs *v;
    char *name = __getfilebuffer();
    int r;
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->rmdir) {
#ifdef _DEBUG
        __builtin_printf("rmdir: ENOSYS: vfs=%x\n", (unsigned)v);
#endif        
        return _seterror(ENOSYS);
    }
    r = (*v->rmdir)(name);
    if (r != 0) {
        return _seterror(-r);
    }
    return r;
}

int
mkdir(const char *orig_name, mode_t mode)
{
    int r;
    struct vfs *v;
    char *name = __getfilebuffer();
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->open) {
#ifdef _DEBUG
        __builtin_printf("rmdir: ENOSYS: vfs=%x\n", (unsigned)v);
#endif        
        return _seterror(ENOSYS);
    }
    r = (*v->mkdir)(name, mode);
    if (r != 0) {
        return _seterror(-r);
    }
    return r;
}
