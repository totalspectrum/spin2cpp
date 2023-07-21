/*
 * dummy system() function that does nothing
 * Written by Eric R. Smith, placed in the public domain
 */
#include <stdlib.h>
#include <errno.h>

int system(const char *cmd)
{
    if (!cmd) {
        return 0; // no shell available
    }
    return _seterror(ENOSYS);
}
