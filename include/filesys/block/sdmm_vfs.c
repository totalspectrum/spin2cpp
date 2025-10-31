#include <stdint.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

vfs_file_t *
_sdmm_open(int pclk, int pss, int pdi, int pdo)
{
    int r;
    int drv = 0;
    struct __using("filesys/block/sdmm.cc") *SDMM;
    unsigned long long pmask;
    vfs_file_t *handle;
    
    SDMM = _gc_alloc_managed(sizeof(*SDMM));

#ifdef _DEBUG
    __builtin_printf("sdmm_open: using pins: %d %d %d %d\n", pclk, pss, pdi, pdo);
#endif    
    pmask = (1ULL << pclk) | (1ULL << pss) | (1ULL << pdi) | (1ULL << pdo);
    if (!_usepins(pmask)) {
        _gc_free(SDMM);
        _seterror(EBUSY);
        return 0;
    }
    SDMM->f_pinmask = pmask;
    r = SDMM->disk_setpins(drv, pclk, pss, pdi, pdo);
    if (r == 0)
        r = SDMM->disk_initialize(0);
    if (r != 0) {
#ifdef _DEBUG
       __builtin_printf("sd card initialize: result=[%d]\n", r);
       _waitms(1000);
#endif
       goto cleanup_and_out;
    }
    handle = _get_vfs_file_handle();
    if (!handle) goto cleanup_and_out;

    handle->flags = O_RDWR;
    handle->bufmode = _IONBF;
    handle->state = _VFS_STATE_INUSE | _VFS_STATE_WROK | _VFS_STATE_RDOK;
    handle->read = &SDMM->v_read;
    handle->write = &SDMM->v_write;
    handle->close = &SDMM->v_close;
    handle->ioctl = &SDMM->v_ioctl;
    handle->flush = &SDMM->v_flush;
    handle->lseek = &SDMM->v_lseek;
    handle->putcf = &SDMM->v_putc;
    handle->getcf = &SDMM->v_getc;
    return handle;
cleanup_and_out:
    _freepins(pmask);
    _gc_free(SDMM);
    _seterror(EIO);
    return 0;
}

struct vfs *
_vfs_open_sdcardx(int pclk, int pss, int pdi, int pdo)
{
    vfs_file_t *handle;
    struct vfs *result;
    
    handle = _sdmm_open(pclk, pss, pdi, pdo);
    if (!handle) return 0;
    result = _vfs_open_fat_handle(handle);
    if (!result) {
        /* go ahead and close the handle */
        handle->close(handle);
    }
    return result;
}

struct vfs *
_vfs_open_sdcard()
{
    return _vfs_open_sdcardx(61, 60, 59, 58);
}

