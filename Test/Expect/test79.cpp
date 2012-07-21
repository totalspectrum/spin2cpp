#include <propeller.h>
#include "test79.h"

int32_t test79::I2c_start(void)
{
  int32_t result = 0;
  _OUTA = ((_OUTA & 0xefffffff) | 0x10000000);
  _DIRA = ((_DIRA & 0xefffffff) | 0x10000000);
  _OUTA = ((_OUTA & 0xdfffffff) | 0x20000000);
  _DIRA = ((_DIRA & 0xdfffffff) | 0x20000000);
  _OUTA &= ~(1<<29);
  _OUTA &= ~(1<<28);
  return result;
}

