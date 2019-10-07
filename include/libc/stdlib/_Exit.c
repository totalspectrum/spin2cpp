#include <propeller.h>

void _Exit(int status)
{
#ifdef __EXIT_STATUS__
    _tx(0xff);
    _tx(0);
    _tx(status);
#endif
    cogstop(cogid());
}

#ifdef __FLEXC__
/* for now make exit() an alias for _Exit */
void exit(int status)
{
    _Exit(status);
}
#endif
