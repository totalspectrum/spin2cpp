#define __SPIN2CPP__
#include <propeller.h>
#include "test081.h"

void test081::High8(void)
{
  _OUTA |= (1 << 8);
  _DIRA |= (1 << 8);
}

void test081::High(int32_t Pin)
{
  _OUTA |= (1 << Pin);
  _DIRA |= (1 << Pin);
}

