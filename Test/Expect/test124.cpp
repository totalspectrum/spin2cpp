#include <propeller.h>
#include "test124.h"

int32_t test124::Getval(void)
{
  return (__builtin_propeller_rev(((_OUTA >> 2) & 0x3), 32 - 2));
}

