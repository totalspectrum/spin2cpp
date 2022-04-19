#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

/* remove a file or directory */
int _remove(const char *orig_name)
{
    int r;
    struct vfs *v;
    char *name = __getfilebuffer();
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->open) {
#ifdef _DEBUG
        __builtin_printf("rmdir: ENOSYS: vfs=%x\n", (unsigned)v);
#endif        
        return _seterror(ENOSYS);
    }
    r = (*v->remove)(name);
    if (r == -EISDIR) {
        r = (*v->rmdir)(name);
    }
    if (r != 0) {
        return _seterror(-r);
    }
    return 0;
}

/* ANSI C version */
int remove(const char *name) {
    return _remove(name);
}
