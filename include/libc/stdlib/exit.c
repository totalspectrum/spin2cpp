#include <stdlib.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

typedef void (*funcptr)(void);

static funcptr exitfuncs[ATEXIT_MAX];
static int numexits;

void exit(int status)
{
    int fd;
    funcptr func;
    
    for (fd = numexits; fd > 0; --fd) {
        if ( (func = exitfuncs[fd-1]) != 0 ) {
            (*func)();
        }
    }
    for (fd = 0; fd < _MAX_FILES; fd++) {
        close(fd);
    }
    _Exit(status);
}

int atexit(void (*function)(void))
{
    if (numexits >= ATEXIT_MAX) {
        return -1;
    }
    exitfuncs[numexits] = function;
    numexits++;
    return 0;
}
