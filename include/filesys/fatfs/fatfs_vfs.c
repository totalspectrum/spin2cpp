#include <stdint.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>

static struct __using("filesys/fatfs/ff.c") FFS;
static FATFS FatFs;

struct vfs *
_vfs_open_sdcard(void)
{
    int r;

    r = FFS.f_mount(&FatFs, "", 0);
    if (r != 0) {
#ifdef DEBUG
       __builtin_printf("fs_init failed: result=[%d]\n", r);
       waitms(1000);
#endif      
       _seterror(-r);
       return 0;
    }
    return FFS.get_vfs();
}
