#include <stdlib.h>
#include <propeller.h>
#include "test81.h"

void test81::High8(void)
{
  OUTA |= (1 << 8);
  DIRA |= (1 << 8);
}

void test81::High(int32_t Pin)
{
  OUTA |= (1 << Pin);
  DIRA |= (1 << Pin);
}

