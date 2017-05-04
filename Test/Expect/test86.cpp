#include <stdlib.h>
#include <propeller.h>
#include "test86.h"

void test86::Set1(void)
{
  OUTA |= (1 << 1);
}

void test86::Set(int32_t Pin)
{
  OUTA |= (1 << Pin);
}

