#include <sys/types.h>

int vfs_file_t::putchar(int c) {
    int i;
    if (!putcf) return 0;
    i = putcf(c, __this);
    return (i < 0) ? 0 : 1;
}

int vfs_file_t::getchar(void) {
    if (!getcf) return -1;
    return getcf(__this);
}
