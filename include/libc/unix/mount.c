#include <sys/vfs.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

//#define _DEBUG

#define MAX_MOUNTS 4
static char *mounttab[MAX_MOUNTS];
static void *vfstab[MAX_MOUNTS];
static char curdir[_PATH_MAX];

char *__getfilebuffer()
{
    static char tmpname[_PATH_MAX];
    return tmpname;
}

struct vfs *
__getvfsforfile(char *name, const char *orig_name)
{
    int i, len;
    struct vfs *v;
    if (orig_name[0] == '/') {
        strncpy(name, orig_name, _PATH_MAX);
    } else {
        strncpy(name, curdir, _PATH_MAX);
        strncat(name, "/", _PATH_MAX);
        strncat(name, orig_name, _PATH_MAX);
    }
    for (i = 0; i < MAX_MOUNTS; i++) {
        if (!mounttab[i]) continue;
#ifdef _DEBUG
        __builtin_printf("_getvfsforfile(%s): trying [%s]\n", name, mounttab[i]);
#endif        
        len = strlen(mounttab[i]);
        if ( (name[len] == '/' || name[len] == 0) && !strncmp(name, mounttab[i], len)) {
            v = (struct vfs *)vfstab[i];
            /* remove prefix */
            strcpy(name, name+len+1);
#ifdef _DEBUG
            __builtin_printf("_getvfsforfile: slot %d returning %x for %s\n", i, (unsigned)v, name);
#endif            
            return v;
        }
    }
    v = _getrootvfs();
#ifdef _DEBUG
    __builtin_printf("_getvfsforfile: returning root %x for %s\n", (unsigned)v, name);
#endif
    return v;
}

int mount(char *name, struct vfs *v)
{
    int i, len;
    int firstfree = -1;

#ifdef _DEBUG
    __builtin_printf("mount(%s, %x) called\n", name, (unsigned)v);
#endif    
    if (name[0] != '/') {
#ifdef _DEBUG
        __builtin_printf("mount %s: EINVAL\n", name);
#endif        
        return _seterror(EINVAL);
    }
    for (i = 0; i < MAX_MOUNTS; i++) {
        if (mounttab[i] == 0) {
            if (firstfree < 0) {
                firstfree = i;
                continue;
            }
        }
        len = strlen(mounttab[i]);
        if (name[len] == '/' && !strncmp(name, mounttab[i], len)) {
            firstfree = i;
            break;
        }
    }
    if (firstfree == -1) {
#ifdef _DEBUG
        __builtin_printf("mount %s: EINVAL\n", name);
#endif        
        return _seterror(EMFILE);
    }
    i = firstfree;
    vfstab[i] = (void *)v;
    if (!v) {
#ifdef _DEBUG
        __builtin_printf("mount: slot %d set empty\n", i);
#endif    
        mounttab[i] = 0;
    } else {
        mounttab[i] = name;
#ifdef _DEBUG
        __builtin_printf("mount: using slot %d for %s\n", i, name);
#endif    
    }
    return 0;
}

char *getcwd(char *buf, size_t size)
{
    size_t needed = 2 + strlen(curdir);

    if (needed > size) {
        _seterror(ERANGE);
        return NULL;
    }
    buf[0] = '/';
    strcpy(buf+1, curdir);
    return buf;
}

int chdir(const char *path)
{
    struct stat s;
    char *tmp;
    int r;
    
    r = stat(path, &s);
    if (r != 0) {
        return r;
    }
    if (!S_ISDIR(s.st_mode)) {
        return _seterror(ENOTDIR);
    }
    tmp = __getfilebuffer();
    __getvfsforfile(tmp, path);
    strcpy(curdir, tmp);
    return 0;
}
