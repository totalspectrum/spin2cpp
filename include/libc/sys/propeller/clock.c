#include <propeller.h>

unsigned int
clock(void)
{
    return getcnt();
}
