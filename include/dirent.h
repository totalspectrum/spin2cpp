#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/limits.h>

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20

#pragma once

struct dirent {
    char d_name[_NAME_MAX]; /** long file name **/
    unsigned long d_off;
    unsigned long d_ino;
    unsigned long d_type; /** directory entry type (ATTR_)**/
    unsigned long d_date; /** date year from 1980 (15:9), month (8:5), day (4:0) **/
    unsigned long d_time; /** time hour (15:11), minutes (10:5), seconds (4:0) **/
    unsigned long d_size; /** file size **/
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
