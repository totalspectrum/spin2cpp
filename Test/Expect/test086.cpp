#include <propeller.h>
#include "test086.h"

void test086::Set1(void)
{
  OUTA |= (1 << 1);
}

void test086::Set(int32_t Pin)
{
  OUTA |= (1 << Pin);
}

