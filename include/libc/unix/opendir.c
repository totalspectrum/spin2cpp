#include <sys/vfs.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

DIR *opendir(const char *orig_name)
{
    struct vfs *v;
    char *name;
    DIR *dir;
    int r;
    name = __getfilebuffer();
    v = (struct vfs *)__getvfsforfile(name, orig_name);
    if (!v || !v->opendir) {
        _seterror(ENOSYS);
        return 0;
    }
    dir = malloc(sizeof(*dir));
    if (!dir) {
        _seterror(ENOMEM);
        return 0;
    }
    r = v->opendir(dir, name);
    if (r) {
        _seterror(r);
        free(dir);
        return 0;
    }
    dir->vfs = v;
    return dir;
}

int closedir(DIR *dir)
{
    int r;
    struct vfs *v = (struct vfs *)dir->vfs;
    r = v->closedir(dir);
    free(dir);
    return _seterror(r);
}

struct dirent *readdir(DIR *dir)
{
    int r;
    struct vfs *v = (struct vfs *)dir->vfs;
    struct dirent *D = &dir->dirent;

    if (!v) {
        return 0;
    }
    r = v->readdir(dir, D);
    if (r) {
        if (r > 0) {
            _seterror(r);
        }
        return 0;
    }
    return D;
}
