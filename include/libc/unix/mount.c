/*
 * high level wrappers for mount() and unmount()
 */
#include <sys/vfs.h>

int mount(const char *user_name, void *v) {
    return _mount((char *)user_name, (struct vfs *)v);
}

int umount(const char *user_name) {
    return _umount((char *)user_name);
}
