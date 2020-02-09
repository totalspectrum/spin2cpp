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
    
    int (*opendir)(vfs_dir_t *dir, const char *name);
    int (*closedir)(vfs_dir_t *dir);
    int (*readdir)(vfs_dir_t *dir, struct dirent *ent);
    int (*stat)(const char *name, struct stat *buf);

    int (*mkdir)(const char *name, mode_t mode);
    int (*rmdir)(const char *name);
    int (*remove)(const char *pathname);
};

struct vfs *_getrootvfs(void) _IMPL("libc/unix/vfs.c");
void _setrootvfs(struct vfs *) _IMPL("libc/unix/vfs.c");

#endif
