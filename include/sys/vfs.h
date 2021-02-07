#ifndef _SYS_VFS_H
#define _SYS_VFS_H

#include <sys/types.h>
#include <dirent.h>

typedef DIR vfs_dir_t;

#pragma once

struct vfs {
    int (*open)(vfs_file_t *fil, const char *name, int flags);
    int (*creat)(vfs_file_t *fil, const char *pathname, mode_t mode);
    int (*close)(vfs_file_t *fil);
    
    ssize_t (*read)(vfs_file_t *fil, void *buf, size_t siz);
    ssize_t (*write)(vfs_file_t *fil, const void *buf, size_t siz);
    off_t (*lseek)(vfs_file_t *fil, off_t offset, int whence);
    int   (*ioctl)(vfs_file_t *fil, unsigned long req, void *argp);
    int (*flush)(vfs_file_t *fil);
    
    int (*opendir)(vfs_dir_t *dir, const char *name);
    int (*closedir)(vfs_dir_t *dir);
    int (*readdir)(vfs_dir_t *dir, struct dirent *ent);
    int (*stat)(const char *name, struct stat *buf);

    int (*mkdir)(const char *name, mode_t mode);
    int (*rmdir)(const char *name);
    int (*remove)(const char *pathname);

    int (*rename)(const char *oldname, const char *newname);
};

int _openraw(void *f, const char *name, unsigned flags, unsigned perm) _IMPL("libc/unix/posixio.c");
int _closeraw(void *f) _IMPL("libc/unix/posixio.c");

// in FlexC these have definitions built in to the system module,
// so no need to _IMPL them
struct vfs *_getrootvfs(void);
void _setrootvfs(struct vfs *);

struct vfs *_vfs_open_host(void) _IMPL("filesys/fs9p/fs9p_vfs.c");
struct vfs *_vfs_open_sdcard(void) _IMPL("filesys/fatfs/fatfs_vfs.c");

/* generic file buffer code */
/* put a "struct _default_buffer" at the start of your vfsdata to use the
 * default versions of putc and getc
 */
#ifdef __P2__
#define _DEFAULT_BUFSIZ 1024
#else
#define _DEFAULT_BUFSIZ 128
#endif

struct _default_buffer {
    int cnt;
    unsigned char *ptr;
    unsigned flags;
    unsigned char buf[_DEFAULT_BUFSIZ];
};

int __default_getc(vfs_file_t *f) _IMPL("libc/unix/bufio.c");
int __default_putc(int c, vfs_file_t *f) _IMPL("libc/unix/bufio.c");
int __default_flush(vfs_file_t *f) _IMPL("libc/unix/bufio.c");

/* directory functions */
char *__getfilebuffer();
struct vfs *__getvfsforfile(char *fullname, const char *orig_name);

#endif
