#include <propeller.h>
#include "cog.h"

unsigned int
clock(void)
{
    return getcnt();
}
