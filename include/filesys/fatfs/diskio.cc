#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <sys/vfs.h>

vfs_file_t *fh;

DRESULT disk_sethandle(BYTE pdrv, vfs_file_t *fhandle) {
    fh = fhandle;
    return RES_OK;
}

DRESULT disk_initialize(BYTE pdrv) {
    if (!fh) {
        // need a handle to use!
        return RES_NOTRDY;
    }
    return RES_OK;
}

DRESULT disk_deinitialize(BYTE pdrv) {
    if (!fh) {
        // need a handle to use!
        return RES_NOTRDY;
    }
    fh->close(fh);
    return RES_OK;
}

DRESULT disk_status(BYTE pdrv) {
    if (!fh) {
        return RES_NOTRDY;
    }
    return RES_OK;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    off_t where = ((off_t)sector) << 9;  /* multiply by 512 */
    off_t rl;
    int r;
    if (!fh) {
        return RES_NOTRDY;
    }
    rl = fh->lseek(fh, where, SEEK_SET);
    if (rl != where) {
        return RES_ERROR;
    }
    count <<= 9; /* multiply by 512 */
    r = fh->read(fh, buff, count);
    if (r != count) {
        return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    off_t where = ((off_t)sector) << 9;  /* multiply by 512 */
    off_t rl;
    UINT r;
    if (!fh) {
        return RES_NOTRDY;
    }
    rl = fh->lseek(fh, where, SEEK_SET);
    if (rl != where) {
        return RES_ERROR;
    }
    count <<= 9; /* multiply by 512 */
    r = fh->write(fh, buff, count);
    if (r != count) {
        return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff)
{
    int r;
    if (!fh) {
        return RES_NOTRDY;
    }
    r = fh->ioctl(fh, ctrl, buff);
    if (r != 0)
        return RES_ERROR;
    return RES_OK;
}
