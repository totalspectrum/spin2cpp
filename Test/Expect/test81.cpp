#include <propeller.h>
#include "test81.h"

int32_t test81::High8(void)
{
  int32_t result = 0;
  _OUTA |= (1<<8);
  _DIRA |= (1<<8);
  return result;
}

int32_t test81::High(int32_t Pin)
{
  int32_t result = 0;
  _OUTA |= (1<<Pin);
  _DIRA |= (1<<Pin);
  return result;
}

