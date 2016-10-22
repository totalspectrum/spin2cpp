#include <propeller.h>
#include "test57.h"

uint8_t test57::dat[] = {
  0x00, 0x00, 0x80, 0x40, 
};
int32_t test57::Getval(void)
{
  return ((int32_t *)&dat[0])[0];
}

