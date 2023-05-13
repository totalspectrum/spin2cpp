/*
 * high level wrappers for mount() and unmount()
 * and similar system calls
 */
#include <sys/vfs.h>

int mount(const char *user_name, void *v) {
    return _mount((char *)user_name, (struct vfs *)v);
}

int umount(const char *user_name) {
    return _umount((char *)user_name);
}

char *getcwd(char *buf, size_t size) {
    return _getcwd(buf, size);
}

int chdir(const char *path) {
    return _chdir(path);
}



