#include <unistd.h>
#include <sys/ioctl.h>

#if defined(__FLEXC__) && !defined(__FEATURE_COMPLEXIO__)
int isatty(int fd) { return 1; }
#else
int isatty(int fd)
{    
    unsigned int flags;
    
    if (ioctl(fd, TTYIOCTLGETFLAGS, &flags) == 0) {
        return 1;
    }
    return 0;
}
#endif


#if defined(__FLEXC__) && !defined(__FEATURE_COMPLEXIO__)
int _isatty(vfs_file_t *f) { return 1; }
#else
int _isatty(vfs_file_t *f)
{    
    unsigned int flags;
    
    if (_ioctl(f, TTYIOCTLGETFLAGS, &flags) == 0) {
        return 1;
    }
    return 0;
}
#endif
