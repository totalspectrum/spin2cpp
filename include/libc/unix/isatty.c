#include <unistd.h>
#include <sys/ioctl.h>

int isatty(int fd)
{
    unsigned int flags;
    
    if (ioctl(fd, TTYIOCTLGETFLAGS, &flags) == 0) {
        return 1;
    }
    return 0;
}
