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
