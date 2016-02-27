#include <propeller.h>
#include "test69.h"

void test69::Demo(void)
{
  if (!(((INA >> 0) & 0x1))) {
    clkset(0x80, 0);
  } else {
    if (!(((INA >> 1) & 0x1))) {
      OUTA &= (~(7 << 0));
    }
  }
}

