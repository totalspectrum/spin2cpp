#include <propeller.h>
#include "test082.h"

void test082::Flip(void)
{
  X = ~X;
}

void test082::Toggle(int32_t Pin)
{
  _OUTA ^= (1 << Pin);
}

