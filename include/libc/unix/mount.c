#include <sys/vfs.h>
#include <sys/limits.h>
#include <stdio.h>
#include <errno.h>

char *__getfilebuffer()
{
    static char tmpname[_PATH_MAX];
    return tmpname;
}

struct vfs *
__getvfsforfile(char *name, const char *orig_name)
{
    strncpy(name, orig_name, _PATH_MAX);
    return _getrootvfs();
}

static char curdir[_PATH_MAX];

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
