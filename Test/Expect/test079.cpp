#define __SPIN2CPP__
#include <propeller.h>
#include "test079.h"

void test079::I2c_start(void)
{
  _OUTA |= (1 << Scl);
  _DIRA |= (1 << Scl);
  _OUTA |= (1 << Sda);
  _DIRA |= (1 << Sda);
  _OUTA &= (~(1 << Sda));
  _OUTA &= (~(1 << Scl));
}

