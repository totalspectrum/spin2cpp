#include <stdlib.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

void exit(int status)
{
    int fd;

    for (fd = 0; fd < _MAX_FILES; fd++) {
        close(fd);
    }
    _Exit(status);
}
