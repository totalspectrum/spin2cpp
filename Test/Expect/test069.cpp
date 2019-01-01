#include <propeller.h>
#include "test069.h"

void test069::Demo(void)
{
  if (!((_INA >> 0) & 0x1)) {
    clkset(0x80, 0);
  } else {
    if (!((_INA >> 1) & 0x1)) {
      _OUTA &= (~(7 << 0));
    }
  }
}

