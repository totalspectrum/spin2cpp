#include <stdint.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>
#include "ff.h"

struct vfs *
_vfs_open_sdcardx(int pclk, int pss, int pdi, int pdo)
{
    int r;
    struct vfs *v;
    int drv = 0;
    struct __using("filesys/fatfs/fatfs.cc") *FFS;
    FATFS *FatFs;

    FFS = _gc_alloc_managed(sizeof(*FFS));
    FatFs = _gc_alloc_managed(sizeof(*FatFs));
    
    r = FFS->disk_setpins(drv, pclk, pss, pdi, pdo);
    if (r == 0) {
        r = FFS->f_mount(FatFs, "", 0);
    }
    if (r != 0) {
#ifdef _DEBUG
       __builtin_printf("sd card fs_init failed: result=[%d]\n", r);
       _waitms(1000);
#endif      
       _seterror(-r);
       return 0;
    }
    v = FFS->get_vfs(FFS);
#ifdef _DEBUG
    {
        unsigned *ptr = (unsigned *)v;
        __builtin_printf("sd card get_vfs: returning %x\n", (unsigned)ptr);
    }
#endif
    return v;
}

struct vfs *
_vfs_open_sdcard()
{
    return _vfs_open_sdcardx(61, 60, 59, 58);
}
