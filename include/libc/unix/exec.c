//
// simple exec() implementation for FlexProp
// Copyright 2021 Total Spectrum Software Inc.
// MIT Licensed
//
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <propeller.h>
#include <errno.h>

// this function must be located in internal memory
// because it trashes HUB memory
void __run(long *src, int size, void *arg) __attribute__((cog))
{
    long *dst = (long *)0;
    long clkmode;
    long origSize = (size + 3)>>2;
    size = origSize;
    do {
        *dst++ = *src++;
        --size;
    } while (size != 0);
#ifdef __P2__
    size = (480*1024)/4-origSize;
    if (size > 0) {
        do {
            *dst++ = 0;
            --size;
        } while (size != 0);
    }
    // set the clock back to RCFAST
    clkmode = __clkmode_var & ~3;
    if (clkmode) {
        __asm {
            hubset clkmode
            waitx  ##20_000_000/100
        }
    }
#else    
    size = (30*1024)/4-origSize;
    if (size > 0) {
        do {
            *dst++ = 0;
            --size;
        } while (size != 0);
    }
#endif    
    _coginit(_cogid(), (void *)0, arg);
}

//
// for now the arguments and environment are ignored
// someday we'll implement them!
//
int _execve(const char *filename, char **argv, char **envp)
{
    vfs_file_t *tab = __getftab(0);
    int fd;
    int r;
    vfs_file_t *f;
    
    // find an empty slot... actually this
    // probably doesn't matter since we're
    // replacing the current program, but
    // just in case we fail...
    for (fd = 0; fd < _MAX_FILES; fd++) {
        if (tab[fd].state == 0) break;
    }
    if (fd == _MAX_FILES) {
        return _seterror(EMFILE);
    }
    f = &tab[fd];
    r = _openraw(f, filename, O_RDONLY, 0644);
    if (r != 0) {
        return r;
    }
    return _vfsexecve(f, argv, envp);
}

int _fexecve(int fd, char **argv, char **envp)
{
    vfs_file_t *f = __getftab(fd);
    if (!f) {
        return _seterror(EBADF);
    }
    return _vfsexecve(f, argv, envp);
}

int _vfsexecve(vfs_file_t *f, char **argv, char **envp)
{
    char *buf, *ptr, *topmem;
    int sizeleft;
    int r;

    // OK, now read the file contents into memory
    // we need to find unused RAM, which basically means after our stack
    // the stack pointer is obtained via
    buf = (char *)__topofstack(0) + 1024;
#ifdef __P2__
    topmem = (char *)0x7fc000;
#else
    topmem = (char *)0x8000;
#endif
    sizeleft = topmem - buf;
    ptr = buf;

    while (sizeleft > 0) {
        r = _vfsread(f, ptr, sizeleft);
        if (r == 0) break;
        if (r < 0)    
            return r;
        ptr += r;
        sizeleft -= r;
    }
    if (sizeleft == 0) {
        // not enough RAM
        return _seterror(ENOMEM);
    }
    sizeleft = ptr - buf;
    if (sizeleft == 0) {
        // empty file
        return _seterror(EACCES);
    }
    // OK, now copy the memory and jump to the new program
    // never returns
    __run((long *)buf, sizeleft, (void *)0);
    // we will not reach here, but the compiler doesn't know that
    return 0;
}
