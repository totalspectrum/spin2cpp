#include <sys/vfs.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

//#define _DEBUG

int stat(const char *orig_name, struct stat *buf)
{
    struct vfs *v;
    char *name = __getfilebuffer();
    int r;
    
#ifdef _DEBUG
    __builtin_printf("stat(%s)\n", orig_name);
#endif    
    v = __getvfsforfile(name, orig_name, NULL);
    if (!v || !v->stat) {
#ifdef _DEBUG
        __builtin_printf("stat: ENOSYS : name(%s) orig_name(%s)\n", name, orig_name);
#endif        
        return _seterror(ENOSYS);
    }
    memset(buf, 0, sizeof(*buf));
#ifdef _DEBUG
    {
        unsigned *ptr = (unsigned *)v->stat;
        __builtin_printf("v == %x\n", (unsigned)v);
        __builtin_printf("calling v->stat %x (%x : %x)\n", (unsigned)ptr, ptr[0], ptr[1]);
    }
#endif    
    r = v->stat(name, buf);
#ifdef _DEBUG
    __builtin_printf("stat: returning %d\n", r);
#endif    
    return r;
}
