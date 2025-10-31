#include <stdint.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "ff.h"


struct vfs *
_vfs_open_fat_handle(vfs_file_t *fhandle)
{
    int r;
    struct vfs *v;
    int drv = 0;
    struct __using("filesys/fatfs/fatfs.cc") *FFS;
    FATFS *FatFs;
    unsigned long long pmask;

    if (!fhandle) {
        _seterror(EBADF);
        return 0;
    }
    
    FFS = _gc_alloc_managed(sizeof(*FFS));
    FatFs = _gc_alloc_managed(sizeof(*FatFs));

    FFS->disk_sethandle(0, fhandle);
    r = FFS->f_mount(FatFs, "", 0);
    if (r != 0) {
        _seterror(-r);
        return 0;
    }
    v = FFS->get_vfs(FFS);
#ifdef _DEBUG
    {
        unsigned *ptr = (unsigned *)v;
        __builtin_printf("FAT get_vfs: returning %x\n", (unsigned)ptr);
    }
#endif
    return v;
}

struct vfs *
_vfs_open_fat_file(const char *name)
{
    int fd;
    vfs_file_t *fhandle;

    fd = open(name, O_RDWR, 0666);
    if (fd < 0) {
        return 0;
    }
    fhandle = __getftab(fd);
    return _vfs_open_fat_handle(fhandle);
}
