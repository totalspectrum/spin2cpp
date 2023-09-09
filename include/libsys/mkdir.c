#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

/* create a directory with default mode */
int _mkdir(const char *orig_name)
{
    int r;
    struct vfs *v;
    char *name = __getfilebuffer();
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->open) {
#ifdef _DEBUG
        __builtin_printf("mkdir: ENOSYS: vfs=%x\n", (unsigned)v);
#endif        
        return _seterror(ENOSYS);
    }
    r = (*v->mkdir)(name, 0777);
    if (r != 0) {
        return _seterror(-r);
    }
    return 0;
}
