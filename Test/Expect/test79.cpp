#include <propeller.h>
#include "test79.h"

void test79::I2c_start(void)
{
  OUTA = (OUTA & 0xefffffff) | 0x10000000;
  DIRA = (DIRA & 0xefffffff) | 0x10000000;
  OUTA = (OUTA & 0xdfffffff) | 0x20000000;
  DIRA = (DIRA & 0xdfffffff) | 0x20000000;
  OUTA &= (~(1 << Sda));
  OUTA &= (~(1 << Scl));
}

