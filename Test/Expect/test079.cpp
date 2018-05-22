#include <propeller.h>
#include "test079.h"

void test079::I2c_start(void)
{
  OUTA |= (1 << Scl);
  DIRA |= (1 << Scl);
  OUTA |= (1 << Sda);
  DIRA |= (1 << Sda);
  OUTA &= (~(1 << Sda));
  OUTA &= (~(1 << Scl));
}

