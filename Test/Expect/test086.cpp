#define __SPIN2CPP__
#include <propeller.h>
#include "test086.h"

void test086::Set1(void)
{
  _OUTA |= (1 << 1);
}

void test086::Set(int32_t Pin)
{
  _OUTA |= (1 << Pin);
}

