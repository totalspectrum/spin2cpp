#include <propeller.h>
#include "test52.h"

int32_t test52::func(void)
{
  int32_t result = 0;
  return ((_INA >> 1) & 0x3);
}

