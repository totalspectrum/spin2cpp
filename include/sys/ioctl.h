#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#define TTYIOCTLGETFLAGS 0x0100
#define TTYIOCTLSETFLAGS 0x0101

#define TTY_FLAG_ECHO 0x01
#define TTY_FLAG_CRNL 0x02

#define BLKIOCTLERASE 0x0200

struct _blkio_info {
    unsigned addr;
    unsigned size;
};

#endif
