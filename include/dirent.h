#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/limits.h>

#pragma once

struct dirent {
    char d_name[_NAME_MAX];
    unsigned long d_off;   // opaque value used for walking the directory
    unsigned long d_ino;   // file inode dummy in our code
    unsigned long d_size;  // file size (NON-STANDARD)
    unsigned long d_mtime; // file modified time (NON-STANDARD)
    unsigned char d_type;  // file type (SEMI-STANDARD)
};

// file types
#define DT_REG   (0)     /* regular file */
#define DT_DIR   (1)     /* directory */
#define DT_UNKNOWN (255) /* unknown file type */

typedef struct _dir {
    void *vfs;
    void *vfsdata;
    struct dirent dirent;
} DIR;

DIR *opendir(const char *name) _IMPL("libc/unix/opendir.c");
int closedir(DIR *dir) _IMPL("libc/unix/opendir.c");
struct dirent *readdir(DIR *dirp) _IMPL("libc/unix/opendir.c");

#endif
