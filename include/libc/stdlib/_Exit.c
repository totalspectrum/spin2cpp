#include <propeller.h>

void _Exit(int status)
{
    cogstop(cogid());
}

#ifdef __FLEXC__
/* for now make exit() an alias for _Exit */
void exit(int status)
{
    _Exit(status);
}
#endif
