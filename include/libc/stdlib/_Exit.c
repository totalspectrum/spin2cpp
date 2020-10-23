#include <propeller.h>

void _Exit(int status)
{
#ifdef __EXIT_STATUS__
    _tx(0xff);
    _tx(0);
    _tx(status);
    _waitx(CLKFREQ / 16); // wait >10ms for all characters to be transmitted
#endif
    _cogstop(_cogid());
}
