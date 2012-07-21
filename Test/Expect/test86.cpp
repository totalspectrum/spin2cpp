#include <propeller.h>
#include "test86.h"

int32_t test86::Set1(void)
{
  int32_t result = 0;
  _OUTA = ((_OUTA & 0xfffffffd) | 0x2);
  return result;
}

int32_t test86::Set(int32_t Pin)
{
  int32_t result = 0;
  _OUTA = ((_OUTA & (~(1 << Pin))) | (1 << Pin));
  return result;
}

