#include <propeller.h>
#include "test07.h"

int32_t test07::Donext(void)
{
  int32_t result = 0;
  Count = (Count + 1);
  return Count;
}

