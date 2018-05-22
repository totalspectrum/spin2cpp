#include <propeller.h>
#include "test081.h"

void test081::High8(void)
{
  OUTA |= (1 << 8);
  DIRA |= (1 << 8);
}

void test081::High(int32_t Pin)
{
  OUTA |= (1 << Pin);
  DIRA |= (1 << Pin);
}

