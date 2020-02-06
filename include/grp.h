#ifndef _GRP_H
#define _GRP_H

struct group {
    char *gr_name;
    gid_t gr_gid;
    char **gr_mem;
};

#endif
