/* placeholder sys/types.h */

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <sys/size_t.h>
#include <sys/wchar_t.h>
#include <time.h> /* for time_t */

#ifndef __OFF_T_DEFINED__
typedef long off_t;
#define __OFF_T_DEFINED__
#endif
#ifndef __SSIZE_T_DEFINED__
typedef long ssize_t;
#define __SSIZE_T_DEFINED__
#endif

typedef int dev_t;
typedef int ino_t;
typedef unsigned int mode_t;

typedef unsigned short uid_t;
typedef unsigned short gid_t;

typedef int pid_t;

struct stat {
  int st_dev;  /* ID of device containing file */
  int st_ino;  /* inode number */
  unsigned int st_mode; /* protection */
  int st_nlink;
  uid_t st_uid;
  gid_t st_gid;
  int st_rdev;
  long st_size;
  long st_blksize;
  long st_blocks;
  time_t st_atime;
  time_t st_mtime;
  time_t st_ctime;
};

typedef struct s_vfs_file_t vfs_file_t;

struct s_vfs_file_t {
    void *vfsdata;
    unsigned flags; /* O_XXX for rdwr mode and such */
    unsigned state; /* flags for EOF and the like */
    int      lock;  /* lock for multiple I/O */
    int      ungot;

    ssize_t (*read)(vfs_file_t *fil, void *buf, size_t count);
    ssize_t (*write)(vfs_file_t *fil, const void *buf, size_t count);
    int (*putcf)(int c, vfs_file_t *fil);
    int (*getcf)(vfs_file_t *fil);
    int (*close)(vfs_file_t *fil);
    int (*ioctl)(vfs_file_t *fil, int arg, void *buf);
    int (*flush)(vfs_file_t *fil);
    off_t (*lseek)(vfs_file_t *fil, off_t offset, int whence);
    
    /* internal functions for formatting routines */
    int putchar(int c) __fromfile("libsys/vfs.c");
    int getchar(void)  __fromfile("libsys/vfs.c");
};

typedef int (*putcfunc_t)(int c, vfs_file_t *fil);
typedef int (*getcfunc_t)(vfs_file_t *fil);

#define _VFS_STATE_RDOK (0x01)
#define _VFS_STATE_WROK (0x02)
#define _VFS_STATE_INUSE (0x04)
#define _VFS_STATE_EOF (0x10)
#define _VFS_STATE_ERR (0x20)
#define _VFS_STATE_APPEND (0x40)
#define _VFS_STATE_NEEDSEEK (0x80)
#define _VFS_STATE_ISATTY (0x100)

#endif
