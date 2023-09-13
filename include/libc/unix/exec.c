//
// simple exec() implementation for FlexProp
// Copyright 2021 Total Spectrum Software Inc.
// MIT Licensed
//
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/argv.h>
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
    int r, j;

    // OK, now read the file contents into memory
    // we need to find unused RAM, which basically means after our stack
    // the stack pointer is obtained via
    buf = (char *)__topofstack(0) + 1024;
#ifdef __P2__
    topmem = (char *)0x7c000;
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
    if (sizeleft <= 0) {
        // not enough RAM
        return _seterror(ENOMEM);
    }
    sizeleft = ptr - buf;
    if (sizeleft == 0) {
        // empty file
        return _seterror(EACCES);
    }
#ifdef __P2__    
    // set up ARGv
    ptr = START_ARGS;
    *(long *)ARGV_ADDR = ARGV_MAGIC;
    // we skip argv[0] (the program name)
    if (argv && argv[0] && argv[1]) {
        int n = 1;
        int ch;
        while (n < MAX_ARGC && argv[n]) {
            char *src = argv[n];
            for (j = 0; 0 != (ch = *src++) && ptr < END_ARGS-2; ptr++) {
                *ptr = ch;
            }
            *ptr++ = 0;
            n++;
            if (ptr >= END_ARGS-1) break;
        }
        *ptr++ = 0;
    }
    *ptr = 0;
    unsigned long *p2 = (unsigned long *)ARGV_ADDR;
    //__builtin_printf("execv: %06x: %08x %08x\n", p2[0], p2[1], p2[2]); 
#endif
    // OK, now copy the memory and jump to the new program
    // never returns
    __run((long *)buf, sizeleft, (void *)0);
    // we will not reach here, but the compiler doesn't know that
    return 0;
}
