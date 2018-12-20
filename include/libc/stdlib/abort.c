#include <stdlib.h>

#ifdef __FLEXC__
void
_Exit(int c)
{
    __asm {
        mov return1, c
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
