//
// Wrapper for Parallax Flash File System
// Written by Eric R. Smith, Total Spectrum Software Inc.
// Released into the public domain.
//

#include <stdint.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <errno.h>

static struct __using("filesys/parallax/parallaxfs_internal.cc") FS;

enum {
    PFS_pclk = 61,
    PFS_pss = 60,
    PFS_pdi = 59,
    PFS_pdo = 58
};

struct vfs *
_vfs_open_parallaxfs(void)
{
    int r;
    struct vfs *v;
    unsigned long long pmask = (1ULL << PFS_pclk) | (1ULL << PFS_pss) | (1ULL << PFS_pdi) | (1ULL << PFS_pdo);

#ifdef _DEBUG
    __builtin_printf("vfs_open_parallax called\n");
#endif

    if (!_usepins(pmask)) {
        _seterror(EBUSY);
        return 0;
    }
    r = FS.fs_init();
    if (r != 0) {
#ifdef _DEBUG
       __builtin_printf("fs_init failed: result=[%d]\n", r);
#endif      
       _seterror(-r);
       return 0;
    }
    v = FS.get_vfs();
#ifdef _DEBUG
    __builtin_printf("_vfs_open_parallax: returning %x\n", (unsigned)v);
    __builtin_printf("v->open == %x\n", (unsigned)(v->open));
#endif    
    return v;
}

int
_mkfs_parallaxfs(void)
{
    unsigned long long pmask = (1ULL << PFS_pclk) | (1ULL << PFS_pss) | (1ULL << PFS_pdi) | (1ULL << PFS_pdo);

#ifdef _DEBUG
    __builtin_printf("vfs_open_parallax called\n");
#endif

    if (!_usepins(pmask)) {
        return _seterror(EBUSY);
    }

    int r = FS.mkfs();
    _freepins(pmask);
    return _seterror(-r);
}
