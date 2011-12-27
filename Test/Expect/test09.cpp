#include <propeller.h>
#include "test09.h"

int32_t test09::init(void)
{
  int32_t result = 0;
  _DIRA = (_DIRA & 0xfffffffb) | ((1 << 2) & 0x00000004);
  _OUTA = (_OUTA & 0xffffff0f) | ((10 << 4) & 0x000000f0);
  return result;
}

