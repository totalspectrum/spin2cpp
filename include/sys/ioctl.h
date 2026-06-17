#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/* disk control ioctls; must match up with defines
   in include/filesys/fatfs/diskio.h
*/
#define DISKIO_CTRL_SYNC        0x0000
#define DISKIO_GET_SECTOR_COUNT 0x0001
#define DISKIO_GET_SECTOR_SIZE  0x0002
#define DISKIO_GET_BLOCK_SIZE   0x0003
#define DISKIO_CTRL_TRIM        0x0004
#define DISKIO_CTRL_FORMAT      0x0005
#define DISKIO_CTRL_POWER_IDLE  0x0006
#define DISKIO_CTRL_POWER_OFF   0x0007
#define DISKIO_CTRL_LOCK        0x0008
#define DISKIO_CTRL_UNLOCK      0x0009
#define DISKIO_CTRL_EJECT       0x000a
#define DISKIO_CTRL_GET_SMART   0x000b

#define DISKIO_GET_STATUS       0x00ff
/* disk status bits */
#  define DISKIO_STA_NOINIT		0x01	/* Drive not initialized */
#  define DISKIO_STA_NODISK		0x02	/* No medium in the drive */
#  define DISKIO_STA_PROTECT		0x04	/* Write protected */

/* tty control */
#define TTYIOCTLGETFLAGS 0x0100
#define TTYIOCTLSETFLAGS 0x0101

#define TTY_FLAG_ECHO 0x01
#define TTY_FLAG_CRNL 0x02

/* flash block control */
#define BLKIOCTLERASE 0x0200

struct _blkio_info {
    unsigned addr;
    unsigned size;
};

#endif
