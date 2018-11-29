// rsimple program to toggle a pin
#include <propeller.h>
#include "btest005.h"

void btest005::pausems(uint32_t ms)
{
  uint32_t 	tim;
  tim = ms * mscycles;
  waitcnt(tim + getcnt());
}

void btest005::program(void)
{
  DIRA |= (1 << lopin);
  OUTA |= (1 << lopin);
  do {
    pausems(500);
    OUTA ^= 0x10000;
  } while (1);
}

