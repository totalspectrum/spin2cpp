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
__getvfsforfile(char *name, const char *orig_name, char *full_path = 0)
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
            /* remove any leading ./ */
            while (name[len+1] == '.' && (name[len+2] == '/' || name[len+2] == 0)) {
                len++;
            }
            /* remove prefix */
            if (full_path) {
                strncpy(full_path, name, _PATH_MAX);
            }
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

int _mount(char *name, struct vfs *v)
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
    if (curdir[0] == '/') {
        strcpy(buf, curdir);
    } else {
        strcpy(buf, "/");
    }
    return buf;
}

int chdir(const char *path)
{
    struct stat s;
    char *tmp;
    int r;
    
    r = stat(path, &s);
    if (r != 0) {
#ifdef _DEBUG
        __builtin_printf("chdir: stat failed\n");
#endif        
        return r;
    }
    if (!S_ISDIR(s.st_mode)) {
#ifdef _DEBUG
        __builtin_printf("chdir: not a directory\n");
#endif        
        return _seterror(ENOTDIR);
    }
    if (path[0] == '/') {
        strncpy(curdir, path, _PATH_MAX);
    } else {
        tmp = __getfilebuffer();
        strncpy(tmp, curdir, _PATH_MAX);
        __getvfsforfile(tmp, path, curdir);
    }
    return 0;
}
