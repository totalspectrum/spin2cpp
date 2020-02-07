#include <sys/vfs.h>

static struct vfs *_rootvfs;

struct vfs *_getrootvfs(void) {
    return _rootvfs;
}

void _setrootvfs(struct vfs *v) {
    _rootvfs = v;
}
