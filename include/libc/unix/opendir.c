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
    v = (struct vfs *)__getvfsforfile(name, orig_name, NULL);
    if (!v || !v->opendir) {
#ifdef _DEBUG
        __builtin_printf("no opendir in vfs\n");
#endif        
        _seterror(ENOSYS);
        return 0;
    }
    dir = malloc(sizeof(*dir));
    if (!dir) {
#ifdef _DEBUG
        __builtin_printf("opendir: malloc failed\n");
#endif        
        _seterror(ENOMEM);
        return 0;
    }
    r = v->opendir(dir, name);
    if (r) {
#ifdef _DEBUG
        __builtin_printf("opendir: v->opendir(%s) returned %d\n", name, r);
#endif        
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
