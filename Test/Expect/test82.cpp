#include <stdlib.h>
#include <propeller.h>
#include "test82.h"

void test82::Flip(void)
{
  X = ~X;
}

void test82::Toggle(int32_t Pin)
{
  OUTA ^= (1 << Pin);
}

