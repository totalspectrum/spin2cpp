#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/limits.h>

#pragma once

typedef struct _dir {
    int fd;
} DIR;

struct dirent {
    char d_name[_NAME_MAX];
    unsigned long d_off;
    unsigned long d_ino;
};

#endif
