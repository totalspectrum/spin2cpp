#include <stdlib.h>
#include <propeller.h>
#include "test79.h"

void test79::I2c_start(void)
{
  OUTA |= (1 << Scl);
  DIRA |= (1 << Scl);
  OUTA |= (1 << Sda);
  DIRA |= (1 << Sda);
  OUTA &= (~(1 << Sda));
  OUTA &= (~(1 << Scl));
}

