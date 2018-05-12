#include <propeller.h>
#include "test52.h"

int32_t test52::Func(void)
{
  return ((INA >> 1) & 0x3);
}

