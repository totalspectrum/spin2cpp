#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

int _ioctl(vfs_file_t *fil, unsigned long req, void *argp)
{
    int r;
    if (!fil) {
        return _seterror(EBADF);
    }
    r = (*fil->ioctl)(fil, req, argp);
    if (r) {
        return _seterror(r);
    }
    return 0;
}

int ioctl(int fd, unsigned long req, void *argp)
{
    vfs_file_t *fil = __getftab(fd);
    return _ioctl(fil, req, argp);
}
