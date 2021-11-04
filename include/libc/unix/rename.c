#include <stdio.h>
#include <sys/limits.h>
#include <sys/vfs.h>
#include <errno.h>

int rename(const char *oldpath, const char *newpath)
{
    int r;
    struct vfs *v, *newv;
    char *oldname = __getfilebuffer();
    char newname[_PATH_MAX];
    
    v = (struct vfs *)__getvfsforfile(oldname, oldpath, NULL);
    if (!v || !v->rename) {
        return _seterror(ENOSYS);
    }
    newv = (struct vfs *)__getvfsforfile(newname, newpath, NULL);
    if (newv != v) {
        return _seterror(EXDEV);
    }
    r = (v->rename)(oldname, newname);
    return _seterror(r);
}
