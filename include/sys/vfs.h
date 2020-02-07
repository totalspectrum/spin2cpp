#ifndef _SYS_VFS_H
#define _SYS_VFS_H

#include <sys/types.h>
#include <dirent.h>

#pragma once

struct vfs {
    int opendir(DIR *dir, const char *name);
    int closedir(DIR *dir);
    int readdir(DIR *dir, struct dirent *ent);
    int stat(const char *name, struct stat *buf);
};

struct vfs *_getrootvfs(void) _IMPL("libc/unix/vfs.c");
void _setrootvfs(struct vfs *) _IMPL("libc/unix/vfs.c");

#endif
