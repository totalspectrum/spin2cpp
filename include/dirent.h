#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/limits.h>

#pragma once

struct dirent {
    char d_name[_NAME_MAX];
    unsigned long d_off;
    unsigned long d_ino;
};

typedef struct _dir {
    void *vfs;
    void *vfsdata;
    struct dirent dirent;
} DIR;

DIR *opendir(const char *name) _IMPL("libc/unix/opendir.c");
int closedir(DIR *dir) _IMPL("libc/unix/opendir.c");
struct dirent *readdir(DIR *dirp) _IMPL("libc/unix/opendir.c");

#endif
