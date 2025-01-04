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

#ifdef __OUTPUT_BYTECODE__
// this function must run in internal memory, because
// it trashes HUB memory
// however, __attribute__((cog)) is ignored in bytecode,
// so we use fcache'd assembly instead
void __run(long *src, int size, void *arg)
{
    long *dst = (long *)0;
    long clkmode;
    long temp;
    long origSize = (size + 3)>>2;
#ifdef __P2__
    long clearSize = (480*1024)/4 - origSize;
#else
    long clearSize = (30*1024)/4 - origSize;
#endif
    __asm volatile {
copyloop
    rdlong temp, src
    add src, #4
    wrlong temp, dst
    add dst, #4
    djnz origSize, #copyloop
    mov temp, #0
clearloop
    wrlong temp, dst
    add dst, #4
    djnz clearSize, #clearloop
#ifdef __P2__
    /* hubset */
    rdlong temp, #24
    andn temp, #3 wz
if_ne hubset temp
if_ne waitx ##200000

    /* and coginit */
    cogid temp
    mov src, #0
    setq arg
    coginit temp, src wc
#else
#error exec not supported in P1 bytecode
#endif
    }
}
#else
// this function must be located in internal memory
// because it trashes HUB memory
void __run(long *src, int size, void *arg) __attribute__((cog))
{
    long *dst = (long *)0;
    long clkmode;
    long origSize = (size + 3)>>2;
#ifdef __P2__    
    long clearSize = (480*1024)/4 - origSize;
#else
    long clearSize = (30*1024)/4 - origSize;
#endif    
    do {
        *dst++ = *src++;
        --origSize;
    } while (origSize != 0);

    while (clearSize > 0) {
        *dst++ = 0;
        --clearSize;
    }
#ifdef __P2__    
    // set the clock back to RCFAST
    clkmode = __clkmode_var & ~3;
    if (clkmode) {
        __asm {
            hubset clkmode
            waitx  ##20_000_000/100
        }
    }
#endif    
    _coginit(_cogid(), (void *)0, arg);
}
#endif

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
#ifdef _DEBUG        
        __builtin_printf("_execve failed, EMFILE\n");
#endif        
        return _seterror(EMFILE);
    }
    f = &tab[fd];
    r = _openraw(f, filename, O_RDONLY, 0644);
    if (r != 0) {
#ifdef _DEBUG        
        __builtin_printf("_execve failed, error %d\n", r);
#endif        
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
    // the stack pointer is obtained via the builtin __topofstack
    buf = (char *)__topofstack(0) + 2048;
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

#ifdef _DEBUG    
    __builtin_printf("vfsexecve read done\n");
#endif
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
