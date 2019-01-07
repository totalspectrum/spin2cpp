#include <stdlib.h>

#ifdef __FLEXC__
void
_Exit(int c)
{
    // FIXME: does not use c
    __asm {
        cogid arg01
        cogstop arg01
    }
}
#endif

void
abort(void)
{
  _Exit(0xff);
}
