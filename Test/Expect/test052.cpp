#include <propeller.h>
#include "test052.h"

int32_t test052::Func(void)
{
  return ((_INA >> 1) & 0x3);
}

