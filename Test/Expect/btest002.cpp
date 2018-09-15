#include <propeller.h>
#include "btest002.h"

void btest002::init(void)
{
  x = 1;
  y = 2;
  z_R = 1.000000;
  w_R = -2.000000;
}

uint32_t btest002::getcycles(void)
{
  return cnt;
}

