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

typedef struct vfs_file_t vfs_file_t;

struct vfs_file_t {
    void *vfsdata;
    unsigned flags;
    ssize_t (*read)(vfs_file_t *fil, void *buf, size_t count);
    ssize_t (*write)(vfs_file_t *fil, const void *buf, size_t count);
    int (*putchar)(int c);
    int (*getchar)(void);
    int (*close)(vfs_file_t *fil);
};

#endif
