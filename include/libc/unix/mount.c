/*
 * high level wrappers for mount() and unmount()
 */
#include <sys/vfs.h>

int mount(char *user_name, void *v) {
    return _mount(user_name, (struct vfs *)v);
}

int umount(char *user_name) {
    return _umount(user_name);
}
