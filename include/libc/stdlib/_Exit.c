#include <propeller.h>

void _Exit(int status)
{
    cogstop(cogid());
}
