#include <sys/vfs.h>
#include <string.h>
#include <errno.h>

int stat(const char *name, struct stat *buf)
{
    struct vfs *root;

    root = _getrootvfs();
    if (!root) {
        return _seterror(ENOSYS);
    }
    memset(buf, 0, sizeof(*buf));
    return root->stat(name, buf);
}
