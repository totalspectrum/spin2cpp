#ifndef _PWD_H
#define _PWD_H

struct passwd {
    char *pw_name; // user's login name
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_dir;
};

#endif
