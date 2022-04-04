#include <sys/vfs.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

//#define _DEBUG

#define MAX_MOUNTS 4
#define MAX_MOUNT_CHARS 15

static char mount_buf[MAX_MOUNTS][MAX_MOUNT_CHARS+1];
static char *mounttab[MAX_MOUNTS];
static void *vfstab[MAX_MOUNTS];
static char curdir[_PATH_MAX];

char *__getfilebuffer()
{
    static char tmpname[_PATH_MAX];
    return tmpname;
}

struct vfs *
__getvfsforfile(char *name, const char *orig_name, char *full_path)
{
    int i, len;
    struct vfs *v;
    if (orig_name[0] == '/') {
        strncpy(name, orig_name, _PATH_MAX);
    } else {
        strncpy(name, curdir, _PATH_MAX);
        if (orig_name[0] == 0 || (orig_name[0] == '.' && orig_name[1] == 0)) {
            // do nothing, we're looking at the directory
        } else {
            strncat(name, "/", _PATH_MAX);
            strncat(name, orig_name, _PATH_MAX);
        }
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

int _mount(char *user_name, struct vfs *v)
{
    int i, len;
    int firstfree = -1;
    struct vfs *oldv;
    
#ifdef _DEBUG
    __builtin_printf("mount(%s, %x) called\n", user_name, (unsigned)v);
#endif    
    if (user_name[0] != '/' || strlen(user_name) > MAX_MOUNT_CHARS) {
#ifdef _DEBUG
        __builtin_printf("mount %s: EINVAL\n", user_name);
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
        if ( (user_name[len] == '/' || user_name[len] == 0) && !strncmp(user_name, mounttab[i], len)) {
            firstfree = i;
            break;
        }
    }
    if (firstfree == -1) {
#ifdef _DEBUG
        __builtin_printf("mount %s: EMFILE\n", user_name);
#endif        
        return _seterror(EMFILE);
    }

    i = firstfree;
    oldv = vfstab[i];
    if (oldv && oldv->deinit) {
        (*oldv->deinit)(mounttab[i]);
    }
    vfstab[i] = (void *)v;
    if (!v) {
#ifdef _DEBUG
        __builtin_printf("mount: slot %d set empty (unmounted %s)\n", i, user_name);
#endif    
        mounttab[i] = 0;
    } else {
        int r = 0;
        char *name;

        name = &mount_buf[i][0];
        // save the name (the user's parameter may not be static)
        strncpy(name, user_name, MAX_MOUNT_CHARS+1);
    
        if (v->init) {
            r = (*v->init)(name);
            if (r) {
                vfstab[i] = 0;
                mounttab[i] = 0;
#ifdef _DEBUG
                __builtin_printf("mount: init failed with error %d for %s\n", r, name);
#endif
                return _seterror(EIO);
            }
        }   
        mounttab[i] = name;
#ifdef _DEBUG
        __builtin_printf("mount: using slot %d for %s\n", i, name);
#endif    
    }
    return 0;
}

int _umount(char *name)
{
    int i, len;
    int firstfree = -1;
    struct vfs *v;
    
#ifdef _DEBUG
    __builtin_printf("umount(%s) called\n", name);
#endif    
    if (name[0] != '/') {
#ifdef _DEBUG
        __builtin_printf("mount %s: EINVAL\n", name);
#endif        
        return _seterror(EINVAL);
    }
    for (i = 0; i < MAX_MOUNTS; i++) {
        if (mounttab[i] == 0) {
            continue;
        }
        len = strlen(mounttab[i]);
#ifdef _DEBUG
        __builtin_printf("umount looking at %d chars of (%s)\n", len, mounttab[i]);
#endif        
        if ( (name[len] == '/' || name[len] == 0) && !strncmp(name, mounttab[i], len)) {
            firstfree = i;
            break;
        }
    }
    if (firstfree == -1) {
#ifdef _DEBUG
        __builtin_printf("umount %s: ENOENT\n", name);
#endif        
        return _seterror(ENOENT);
    }
    i = firstfree;
    v = (struct vfs *)vfstab[i];
    if (v && v->deinit) {
#ifdef _DEBUG
        __builtin_printf("umount: calling deinit\n", i);
#endif
        (*v->deinit)(name);
        vfstab[i] = 0;
    }
    mounttab[i] = 0;
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
