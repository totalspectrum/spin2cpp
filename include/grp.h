#ifndef _GRP_H
#define _GRP_H

struct group {
    char *gr_name;
    gid_t gr_gid;
    char **gr_mem;
};

struct group *getgrgid(gid_t gid) _IMPL("libc/unix/getgrgid.c");

#endif
